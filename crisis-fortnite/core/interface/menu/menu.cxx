#include <deps/includes.h>

namespace menu {
	static constexpr float        panel_w = 910.0f;
	static constexpr float        panel_h = 511.0f;
	static constexpr float        panel_radius = 18.0f;
	static constexpr float        sidebar_w = 195.0f;
	static constexpr float        child_layout_h = 507.0f;
	static constexpr float        nav_rail_w = 58.0f;
	// Rail height sized for main_tabs count. Formula:
	// pad + (N-1)*spacing + item + pad. Widget-layout button
	// was previously included in the count (N=4) — deprecated,
	// so back down to the 3 main tabs (Home / Config / Profile).
	// Bumping this back up is a one-line change if the widget-
	// layout toggle ever comes back.
	static constexpr float        nav_rail_item_size    = 36.0f;
	static constexpr float        nav_rail_item_spacing = 42.0f;
	static constexpr float        nav_rail_button_count = 3.0f; // main tabs only
	static constexpr float        nav_rail_pad          = 8.0f;
	static constexpr float        nav_rail_h =
		nav_rail_pad * 2.0f
		+ ( nav_rail_button_count - 1.0f ) * nav_rail_item_spacing
		+ nav_rail_item_size;
	static constexpr float        nav_rail_gap = 12.0f;
	static float                   s_shake_at = -100.0f;  // namespace-level for cross-function access

	// Layout constants
	static constexpr float pad_x = 20.0f;
	static constexpr float pad_y = 20.0f;
	static constexpr float header_h = 40.0f;

	// ---- Main navigation rail --------------------------------------------------
	struct main_tab_t {
		const wchar_t* icon;
		const wchar_t* label;
		float          tooltip_w;
	};

	static constexpr main_tab_t main_tabs[ ] = {
		{ L"", L"Home",   58.0f },
		{ L"", L"Config", 66.0f },
		{ L"", L"Profile", 66.0f },
	};
	static std::uint32_t s_main_tab = 0;

	// ---- Sidebar data -----------------------------------------------------------
	struct sidebar_sub_t { const wchar_t* label; };
	struct sidebar_tab_t {
		const wchar_t* icon;
		const wchar_t* label;
		const sidebar_sub_t* subs;
		std::uint32_t        sub_count;
	};

	static constexpr sidebar_sub_t aimbot_subs[ ] = { { L"General" }, { L"Triggerbot" }, { L"Curves" } };
	static constexpr sidebar_sub_t visuals_subs[ ] = { { L"General" }, { L"Information" }, { L"World" } };
	static constexpr sidebar_sub_t world_subs[ ] = { { L"General" }, { L"Loot" }, { L"Vehicles" } };
	// New Exploits dropdown added 2026-09-01 — one sub-tab today ("General"),
	// room to grow. Sits alongside Aimbot / Visuals under the Home tab.
	static constexpr sidebar_sub_t exploits_subs[ ] = { { L"General" } };
	static constexpr sidebar_tab_t sidebar_tabs[ ] = {
		{ L"", L"Aimbot",   aimbot_subs,   3 },
		{ L"", L"Visuals",  visuals_subs,   3 },
		{ L"", L"Exploits", exploits_subs, 1 },
	};

	static std::uint32_t s_sidebar_tab = 0;
	static std::uint32_t s_sidebar_subtab = 0;
	static std::uint32_t s_expanded_tabs = 1;
	static std::array<float, std::size( sidebar_tabs )> s_dropdown_progress = { 1.0f, 0.0f, 0.0f };

	// ---- Child panel data -------------------------------------------------------
	struct child_t {
		const wchar_t* icon;
		const wchar_t* title;
		const wchar_t* description;
		bool           enabled;
		float          last_height = 0.0f;
	};

	static child_t aimbot_general_children[ ] = {
		{ L"", L"General",       L"Core aim behaviour and movement", true },
		{ L"", L"Indicators",    L"On-screen aiming guides", true },
		{ L"\uf085", L"Miscellaneous", L"Activation and target filters", true },
	};

	static child_t aimbot_curve_children[ ] = {
		{ L"", L"Curve Engine", L"Humanised aim-path configuration", true },
	};

	static child_t aimbot_trigger_children[ ] = {
		{ L"\ue19b", L"Triggerbot", L"Auto-fire when on target",  false },
		{ L"\uf252", L"Delay",      L"Add a reaction-time delay", false },
	};

	static child_t visuals_children[ ] = {
		{ L"\uf06e", L"Boxes", L"Player box shape and styling", true },  // visuals_children[0]
		{ L"\uf183", L"Skeleton", L"Player skeleton styling", true },  // visuals_children[1]
		// Snaplines icon assignment \u2014 see the next line; the raw L"..." literal is replaced via s_snapline_icon.
		{ L"", L"Snaplines", L"Player snapline placement and styling", true },   /* visuals_children[2] icon patched at runtime */
		{ L"", L"Tracers", L"Player position trails", true },       // visuals_children[3]
		{ L"", L"ESP Preview", L"Live sketch of current ESP settings", true },  // visuals_children[4]
	};

	static child_t visuals_info_children[ ] = {
		{ L"", L"Player Information", L"Identity, rank and match statistics", true },
		{ L"", L"Weapon Information", L"Equipment and ammo labels", true },  /* visuals_info_children[1] icon patched at runtime */  // visuals_info_children[1] — icon set to  (2026-08-26)
		{ L"", L"World Information", L"World-state and threat labels", true },  // visuals_info_children[2] — renamed 2026-08-26 (absorbs threat warning + sound alert)
	};

	static child_t visuals_snapline_children[ ] = {
		{ L"", L"Snapline Placement", L"Enable and position player snaplines", true },
		{ L"", L"Snapline Styling", L"Configure snapline presentation", true },
	};

	static child_t world_general_children[ ] = {
		{ L"\uf0ac", L"World ESP",       L"Enable world-space entity labels",   false },
		{ L"\uf7c0", L"Radar",           L"Mini-map radar + FPS widgets",       true  },
		{ L"\ue54e", L"Bullet Tracers",  L"Local-shot beam visualisation",      true  },
	};

	// Exploits \u25b8 General cards. Wired into the render branch for
	// s_sidebar_tab == 2 && s_sidebar_subtab == 0 below.
	static child_t exploits_general_children[ ] = {
		{ L"\uf0e7", L"Weapon", L"", true },
	};

	static child_t world_loot_children[ ] = {
		{ L"", L"Loot ESP",      L"Highlight ground loot items",           false },
		{ L"", L"Rarity Filter", L"Only show items above a rarity tier",   false },
	};

	static child_t world_vehicles_children[ ] = {
		{ L"", L"Vehicle ESP", L"Label all driveable vehicles",    false },
		{ L"", L"Distance",    L"Show distance to each vehicle",   false },
	};


	// ---- Widget state -----------------------------------------------------------
	// Combo widget — per-label open state (keyed by pointer identity, stable for constexpr strings)
	struct combo_state_t { bool open = false; };
	static std::unordered_map<const wchar_t*, combo_state_t> s_combo_states;
	static std::vector< std::function< void( ) > > s_dropdown_draw_queue;

	// Keybind widget — per-label open state + VK name helper
	enum keybind_status_t : std::uint8_t { KB_NONE = 0, KB_WAITING, KB_ASSIGNED };
	struct keybind_state_t { keybind_status_t status = KB_NONE; bool click_guard = false; };
	static std::unordered_map<const wchar_t*, keybind_state_t> s_keybind_states;

	struct checkbox_animation_t {
		float progress = 0.0f;
		float pulse = 0.0f;
		bool initialized = false;
	};
	struct slider_animation_t {
		float position = 0.0f;
		float grabbed = 0.0f;
		bool initialized = false;
	};
	static std::unordered_map<const bool*, checkbox_animation_t> s_checkbox_animations;
	static std::unordered_map<const float*, slider_animation_t> s_slider_animations;

	// Search and persistent configuration state.
	static std::wstring s_search_query;
	static bool s_search_focused = false;
	static std::array<bool, 256> s_search_key_down {};
	static std::wstring s_config_status = L"Ready";
	static std::wstring s_profile_name = [ ] {
		wchar_t username[ 256 ] {};
		const DWORD length = GetEnvironmentVariableW(
			L"USERNAME", username, static_cast< DWORD >( std::size( username ) ) );
		return std::wstring( length > 0 && length < std::size( username ) ? username : L"User" );
		}( );
	static std::wstring s_profile_username = s_profile_name;
	static bool s_profile_image_registered = false;
	static std::wstring s_profile_discord_id;
	static std::wstring s_profile_role = L"Developer";
	static std::int64_t s_profile_expires_unix = 0;
	static bool s_logo_registered = false;
	static bool s_performance_vsync = true;
	static std::uint32_t s_menu_theme = 0;
	static bool s_show_notifications = true;

	// ── Widget layout mode ───────────────────────────────────────
	// Toggled by the "arrows" button in the sidebar (icon ).
	// When active, render_widget_layout_overlay() is called at the
	// end of every render frame so the overlay lays on top of the
	// menu AND the game world. Rendering itself is framework-only
	// for now — see that function's body for stub sections. Menu
	// interactions stay live while the mode is on; the overlay is
	// purely additive so we can iterate on the visuals without
	// blocking every other menu widget.
	static bool s_widget_layout_mode = false;
	static bool s_show_background = true;
	static bool s_streamproof = false;
	static int s_menu_key = VK_INSERT;
	namespace notifications {
		struct item_t {
			std::wstring header;
			std::wstring description;
			std::wstring icon;
			bypass::overlay::color_t color {};
			float age = 0.0f;
			float current_y = -1.0f;
			bool pinned = false;
			bool centered = false;
		};
		inline std::vector< item_t > queue;
		inline std::mutex queue_mutex;

		void push( std::wstring header, std::wstring description,
			std::wstring icon, bypass::overlay::color_t color,
			bool pinned, bool centered ) {
			if ( !s_show_notifications )
				return;
			std::lock_guard lock( queue_mutex );
			if ( queue.size( ) >= 5 )
				queue.erase( queue.begin( ) );
			item_t it { std::move( header ), std::move( description ),
				std::move( icon ), color };
			it.pinned   = pinned;
			it.centered = centered;
			queue.push_back( std::move( it ) );
		}

		void finish_pinned( ) {
			std::lock_guard lock( queue_mutex );
			for ( auto& it : queue ) {
				if ( !it.pinned ) continue;
				it.age    = 3.74f;
				it.pinned = false;
			}
		}

		void render_pass( ) {
			static auto s_notif_last_tick = std::chrono::steady_clock::now( );
			const auto animation_now = std::chrono::steady_clock::now( );
			float animation_delta = std::chrono::duration< float >(
				animation_now - s_notif_last_tick ).count( );
			if ( animation_delta <= 0.0f || animation_delta > 0.1f )
				animation_delta = 0.016f;
			s_notif_last_tick = animation_now;

			constexpr bypass::overlay::color_t col_bg { 10, 9, 18, 255 };
			const bypass::overlay::color_t col_accent = current_accent( );

			constexpr float notification_w      = 344.0f;
			constexpr float notification_h      =  56.0f;
			constexpr float notification_gap    =  10.0f;
			constexpr float notification_margin =  18.0f;
			constexpr float notification_enter  =   0.34f;
			constexpr float notification_hold   =   3.4f;
			constexpr float notification_exit   =   0.28f;
			const float notification_lifetime   = notification_enter + notification_hold + notification_exit;

			if ( gui::screen_width <= 0 || gui::screen_height <= 0 ) {
				bypass::overlay::get_size( gui::screen_width, gui::screen_height );
				if ( gui::screen_width <= 0 ) gui::screen_width = 1920;
				if ( gui::screen_height <= 0 ) gui::screen_height = 1080;
			}

			std::lock_guard notifications_lock( queue_mutex );
			for ( auto& notification : queue ) {
				if ( !notification.pinned || notification.age < 0.34f )
					notification.age += animation_delta;
			}

			queue.erase( std::remove_if(
				queue.begin( ), queue.end( ),
				[ & ]( const item_t& notification ) {
					return notification.age >= notification_lifetime;
				} ), queue.end( ) );

			for ( std::size_t i = 0; i < queue.size( ); ++i ) {
				auto& notification = queue[ i ];

				float visibility = 1.0f;
				if ( notification.age < notification_enter ) {
					visibility = notification.age / notification_enter;
				}
				else if ( notification.age > notification_enter + notification_hold ) {
					const float t = ( std::clamp ) (
						( notification.age - notification_enter - notification_hold )
						/ notification_exit, 0.0f, 1.0f );
					visibility = 1.0f - t;
				}

				float x, y;
				if ( notification.centered ) {
					x = ( static_cast< float >( gui::screen_width ) - notification_w ) * 0.5f;
					const float sw_f = static_cast< float >( gui::screen_width  );
					const float sh_f = static_cast< float >( gui::screen_height );
					const float rx   = sw_f > 0.f ? sw_f / 1920.f : 1.f;
					const float ry   = sh_f > 0.f ? sh_f / 1080.f : 1.f;
					const float scale = rx < ry ? rx : ry;
					const float rest_y = 48.0f * scale;
					const float drop_d = rest_y + notification_h;
					if ( notification.age < notification_enter ) {
						const float p    = ( std::clamp ) ( notification.age / notification_enter, 0.0f, 1.0f );
						const float ease = 1.0f - ( 1.0f - p ) * ( 1.0f - p );
						y = rest_y - drop_d * ( 1.0f - ease );
					} else if ( notification.age > notification_enter + notification_hold ) {
						const float t    = ( std::clamp ) ( ( notification.age - notification_enter - notification_hold ) / notification_exit, 0.0f, 1.0f );
						y = rest_y - drop_d * t * t;
					} else {
						y = rest_y;
					}
				} else {
					const float target_y = notification_margin + i * ( notification_h + notification_gap );
					if ( notification.current_y < 0.0f )
						notification.current_y = target_y;
					const float stack_ease = 1.0f - std::exp( -animation_delta * 14.0f );
					notification.current_y += ( target_y - notification.current_y ) * stack_ease;

					const float target_x   = static_cast< float >( gui::screen_width )
						- notification_margin - notification_w;
					const float slide_t    = ( std::clamp ) ( notification.age / notification_enter, 0.0f, 1.0f );
					const float slide_ease = 1.0f - ( 1.0f - slide_t ) * ( 1.0f - slide_t );
					x = target_x + ( 1.0f - slide_ease ) * 50.0f;
					y = notification.current_y;
				}

				const bool has_custom_color = notification.color.r != 0
					|| notification.color.g != 0
					|| notification.color.b != 0;
				const bypass::overlay::color_t notification_color = has_custom_color
					? notification.color
					: col_accent;

				auto alpha = [ & ]( float base ) {
					return static_cast< std::uint8_t >( ( std::clamp ) ( base * visibility, 0.0f, 255.0f ) );
				};

				bypass::overlay::filled_rounded_rectangle( { x, y, notification_w, notification_h }, 12.0f, bypass::overlay::color_t { col_bg.r, col_bg.g, col_bg.b, alpha( col_bg.a ) } );
				bypass::overlay::filled_rounded_rectangle( { x, y, notification_w, notification_h }, 12.0f, bypass::overlay::color_t { 28, 25, 40, alpha( 220 ) } );

				constexpr float dot_spacing = 20.0f;
				constexpr float outer_dot_radius = 2.5f;
				const bypass::overlay::point_t pattern_center {
					notification_w * 0.5f, notification_h * 0.5f };
				const float maximum_distance = std::sqrt(
					pattern_center.x * pattern_center.x + pattern_center.y * pattern_center.y );
				const float notification_animation_time = std::chrono::duration< float >(
					animation_now.time_since_epoch( ) ).count( );

				bypass::overlay::push_clip( { x, y, notification_w, notification_h } );
				for ( float dot_y = 0.0f; dot_y < notification_h; dot_y += dot_spacing ) {
					for ( float dot_x = 0.0f; dot_x < notification_w; dot_x += dot_spacing ) {
						const float dx = dot_x - pattern_center.x;
						const float dy = dot_y - pattern_center.y;
						const float distance = std::sqrt( dx * dx + dy * dy );
						const float distance_fade = ( std::max ) (
							0.0f, 1.0f - distance / maximum_distance );
						const float pulse = 0.5f + 0.5f * std::sin(
							notification_animation_time * 2.0f + ( dot_x + dot_y ) * 0.05f );

						const float dot_opacity = 0.85f * distance_fade * pulse;
						if ( dot_opacity <= 0.05f ) continue;

						const auto dot_alpha = alpha( dot_opacity * 255.0f );
						const bypass::overlay::point_t dot { x + dot_x + 1.0f, y + dot_y + 1.0f };

						bypass::overlay::filled_circle( dot, outer_dot_radius,
							bypass::overlay::color_t { notification_color.r, notification_color.g,
								notification_color.b, dot_alpha } );
					}
				}
				bypass::overlay::pop_clip( );

				bypass::overlay::rounded_rectangle(
					{ x, y, notification_w, notification_h }, 12.0f,
					bypass::overlay::color_t {
						notification_color.r, notification_color.g, notification_color.b,
						alpha( 150.0f )
					}, 2.5f );

				bypass::overlay::filled_rounded_rectangle(
					{ x + 11.0f, y + 8.0f, 50.0f, 40.0f }, 11.0f,
					bypass::overlay::color_t { notification_color.r, notification_color.g,
						notification_color.b, alpha( 70.0f ) } );
				bypass::overlay::rounded_rectangle(
					{ x + 11.0f, y + 8.0f, 50.0f, 40.0f }, 11.0f,
					bypass::overlay::color_t { notification_color.r, notification_color.g,
						notification_color.b, alpha( 200.0f ) }, 1.0f );

				bypass::overlay::text_ex( notification.icon.c_str( ),
					{ x + 11.0f, y + 8.0f, 50.0f, 40.0f },
					bypass::overlay::color_t { notification_color.r,
						notification_color.g, notification_color.b,
						alpha( 255.0f ) },
					18.0f, bypass::overlay::text_weight_t::regular, L"Font Awesome",
					bypass::overlay::text_align_t::center, false );

				bypass::overlay::text_ex( notification.header.c_str( ),
					{ x + 73.0f, y + 8.0f, notification_w - 86.0f, 20.0f },
					bypass::overlay::color_t { 255, 255, 255, alpha( 255.0f ) }, 12.5f,
					bypass::overlay::text_weight_t::bold, L"Geist-Bold",
					bypass::overlay::text_align_t::left, false );
				bypass::overlay::text_ex( notification.description.c_str( ),
					{ x + 73.0f, y + 25.0f, notification_w - 86.0f, 20.0f },
					bypass::overlay::color_t { 210, 205, 225, alpha( 235.0f ) }, 11.0f,
					bypass::overlay::text_weight_t::bold, L"Inter-Regular",
					bypass::overlay::text_align_t::left, false );
			}
		}
	}
	struct fortnite_stats_t {
		std::uint32_t games_played = 0;
		std::uint32_t wins = 0;
		std::uint32_t kills = 0;
		std::uint32_t deaths = 0;
		std::uint32_t ranked_games = 0;
		std::uint32_t non_ranked_games = 0;
		std::uint32_t tournament_games = 0;
		std::uint32_t ranked_kills = 0;
		std::uint32_t non_ranked_kills = 0;
		std::uint32_t tournament_kills = 0;
		std::uint32_t accounts_used = 1;
		float average_game_minutes = 0.0f;
	};
	static fortnite_stats_t s_fortnite_stats;
	struct curve_point_t { float x; float y; };
	static bool s_custom_curve = false;
	static std::vector<curve_point_t> s_custom_curve_points = {
		curve_point_t{ 0.0f, 1.0f }, curve_point_t{ 1.0f, 0.0f },
	};
	static int s_dragged_curve_point = -1;

	void set_profile( std::wstring display_name, std::wstring username, bool image_registered,
		std::wstring discord_id, std::wstring role, std::int64_t expires_unix ) {
		if ( !display_name.empty( ) )
			s_profile_name = std::move( display_name );
		if ( !username.empty( ) )
			s_profile_username = std::move( username );
		s_profile_image_registered = image_registered;
		s_profile_discord_id = std::move( discord_id );
		s_profile_role = std::move( role );
		s_profile_expires_unix = expires_unix;

		g_discord_name        = !s_profile_name.empty( ) ? s_profile_name : s_profile_username;
		g_profile_image_ready = image_registered;
	}

	void set_logo_registered( bool image_registered ) {
		s_logo_registered = image_registered;
	}

	int menu_key( ) noexcept {
		return s_menu_key;
	}

	struct config_snapshot_t {
		std::uint32_t magic = 0x43525331;
		std::uint32_t version = 12;

		bool aimbot_enabled, curve, curve_behind_walls;
		bool bezier_curve, bspline_curve, chaikin_curve, staggered_curve;
		bool aim_at_npcs, aim_at_knocked, sticky_target;
		float field_of_view, curve_noise, deadzone, smoothing, acceleration, acceleration_threshold;
		int prediction_type;
		bool curve_line, fov_circle, crosshair;
		bool fov_arrows, prediction_dot;
		bool threat_warning;
		float threat_distance, threat_fov;
		bool triggerbot_enabled, triggerbot_visible_only, triggerbot_fire_without_aim;
		float triggerbot_deadzone_px, triggerbot_max_distance_m;
		float triggerbot_pre_fire_ms, triggerbot_post_fire_ms;
		std::uint64_t triggerbot_weapon_mask;

		bool box_normal, box_corner, box_filled, box_outline;
		bool skeleton_normal, skeleton_curved, skeleton_outline, skeleton_fingers;
		float skeleton_curve_smoothness;
		bool snaplines_enabled, snaplines_outline; int snaplines_position;
		bool player_name, battle_pass, kill_deaths, winrate, rank, platform;
		bool team_marker, streamsnipe, health_bar, hit_markers;
		bool building, distance, inventory;
		bool weapon_name, weapon_damage;
		bool loot_pickup, loot_chest, loot_weakspot;
		float loot_pickup_distance, loot_chest_distance, loot_weakspot_distance;

		// Version 5 fields are appended to preserve the version 4 byte layout.
		bool visible_only = false;
		int aim_key = VK_RBUTTON;
		float max_distance = 300.0f;
		int target_priority = 0;
		float reserved_switch_delay_ms = 0.0f;
		float reserved_visibility_grace_ms = 0.0f;

		// Version 6 fields are appended to preserve the version 5 byte layout.
		// weapon_ammo gates the ammo counter under the weapon-display line.
		bool weapon_ammo = true;

		// v7: reloading tag under the weapon display.
		bool weapon_reloading = false;
		// v8: snapline smoothing (bezier bulge).
		float snaplines_smoothing = 0.35f;
		// v9: which side of the box each HP/shield bar sits on
		// (0 = left, 1 = right, 2 = top, 3 = bottom).
		int health_bar_position = 0;
		int shield_bar_position = 3;
		// v10-v12 per-slot text customisation arrays (slot_count == 13).
		int text_sizes[ 13 ] { 2, 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1 };
		std::uint8_t text_color_override_enabled[ 13 ] { };
		std::uint32_t text_color_override[ 13 ] {
			0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
			0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
			0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
			0xFFFFFFFFu };
		std::uint8_t text_outline_enabled[ 13 ] {
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
		float text_outline_width[ 13 ] {
			1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };
		std::uint8_t text_background_enabled[ 13 ] { };
		std::int32_t text_anchors[ 13 ] { 0, 0, 0, 0, 0, 0, 0,   3, 1, 1, 1, 1,   3 };
		std::int32_t text_orders [ 13 ] { 6, 3, 4, 5, 2, 1, 0,   1, 5, 0, 1, 4,   0 };
	};
	static constexpr std::size_t config_snapshot_v4_size = 152;
	static constexpr std::size_t config_snapshot_v5_size  = offsetof( config_snapshot_t, weapon_ammo );
	static constexpr std::size_t config_snapshot_v6_size  = offsetof( config_snapshot_t, weapon_reloading );
	static constexpr std::size_t config_snapshot_v7_size  = offsetof( config_snapshot_t, snaplines_smoothing );
	static constexpr std::size_t config_snapshot_v8_size  = offsetof( config_snapshot_t, health_bar_position );
	static constexpr std::size_t config_snapshot_v9_size  = offsetof( config_snapshot_t, text_sizes );
	static constexpr std::size_t config_snapshot_v10_size = offsetof( config_snapshot_t, text_background_enabled );
	static constexpr std::size_t config_snapshot_v11_size = offsetof( config_snapshot_t, text_anchors );

	static inline config_snapshot_t capture_config( ) {
		config_snapshot_t out {};
		out.aimbot_enabled = config::aimbot::enabled;
		out.curve = config::aimbot::curve;
		out.curve_behind_walls = config::aimbot::curve_behind_walls;
		out.bezier_curve = config::aimbot::bezier_curve;
		out.bspline_curve = config::aimbot::bspline_curve;
		out.chaikin_curve = config::aimbot::chaikin_curve;
		out.staggered_curve = config::aimbot::staggered_curve;
		out.aim_at_npcs = config::aimbot::aim_at_npcs;
		out.aim_at_knocked = config::aimbot::aim_at_knocked;
		out.sticky_target = config::aimbot::sticky_target;
		out.field_of_view = config::aimbot::field_of_view;
		out.curve_noise = config::aimbot::curve_noise;
		out.deadzone = config::aimbot::deadzone;
		out.smoothing = config::aimbot::smoothing;
		out.acceleration = config::aimbot::acceleration;
		out.acceleration_threshold = config::aimbot::acceleration_threshold;
		out.prediction_type = config::aimbot::prediction_type;
		out.curve_line = config::aimbot::visuals::curve_line;
		out.fov_circle = config::aimbot::visuals::field_of_view;
		out.crosshair = config::aimbot::visuals::crosshair;
		out.fov_arrows = config::aimbot::visuals::fov_arrows;
		out.prediction_dot = config::aimbot::visuals::prediction_dot;
		out.threat_warning = config::aimbot::visuals::threat_warning;
		out.threat_distance = config::aimbot::visuals::threat_distance;
		out.threat_fov = config::aimbot::visuals::threat_fov;
		out.triggerbot_enabled = config::triggerbot::enabled;
		out.triggerbot_visible_only = config::triggerbot::visible_only;
		out.triggerbot_fire_without_aim = config::triggerbot::fire_without_aim;
		out.triggerbot_deadzone_px = config::triggerbot::deadzone_px;
		out.triggerbot_max_distance_m = config::triggerbot::max_distance_m;
		out.triggerbot_pre_fire_ms = config::triggerbot::pre_fire_ms;
		out.triggerbot_post_fire_ms = config::triggerbot::post_fire_ms;
		out.triggerbot_weapon_mask = config::triggerbot::weapon_mask;

		out.box_normal = config::player::box::normal;
		out.box_corner = config::player::box::corner;
		out.box_filled = config::player::box::filled;
		out.box_outline = config::player::box::outline;
		out.skeleton_normal = config::player::skeleton::normal;
		out.skeleton_curved = config::player::skeleton::curved;
		out.skeleton_outline = config::player::skeleton::outline;
		out.skeleton_fingers = config::player::skeleton::fingers;
		out.skeleton_curve_smoothness = config::player::skeleton::curve_smoothness;
		out.snaplines_enabled = config::player::snaplines::enabled;
		out.snaplines_position = config::player::snaplines::position;
		out.snaplines_outline = config::player::snaplines::outline;
		out.player_name = config::player::stats::player_name;
		out.battle_pass = config::player::stats::battle_pass;
		out.kill_deaths = config::player::stats::kill_deaths;
		out.winrate = config::player::stats::winrate;
		out.rank = config::player::stats::rank;
		out.platform = config::player::stats::platform;
		out.team_marker = config::player::stats::team_marker;
		out.streamsnipe = config::player::stats::streamsnipe;
		out.health_bar = config::player::stats::health_bar;
		out.hit_markers = config::player::stats::hit_markers;
		out.building = config::player::stats::state;
		out.distance = config::player::stats::distance;
		out.inventory = config::player::stats::inventory;
		out.weapon_name = config::player::stats::weapon_name;
		out.weapon_damage = config::player::stats::weapon_damage;
		out.weapon_ammo = config::player::stats::weapon_ammo;
		out.loot_pickup = config::loot::pickup;
		out.loot_chest = config::loot::chest;
		out.loot_weakspot = config::loot::weakspot;
		out.loot_pickup_distance = config::loot::pickup_distance;
		out.loot_chest_distance = config::loot::chest_distance;
		out.loot_weakspot_distance = config::loot::weakspot_distance;
		out.visible_only = config::aimbot::visible_only;
		out.aim_key = config::aimbot::aim_key;
		out.max_distance = config::aimbot::max_distance;
		out.target_priority = config::aimbot::target_priority;
		out.reserved_switch_delay_ms = 0.0f;
		out.reserved_visibility_grace_ms = 0.0f;
		out.weapon_reloading    = config::player::stats::weapon_reloading;
		out.snaplines_smoothing = config::player::snaplines::smoothing;
		out.health_bar_position = config::player::stats::health_bar_position;
		out.shield_bar_position = config::player::stats::shield_bar_position;
		for ( int i = 0; i < config::player::stats::slot_count; ++i ) {
			out.text_sizes[ i ]                    = config::player::stats::text_sizes[ i ];
			out.text_color_override_enabled[ i ]   = config::player::stats::text_color_override_enabled[ i ] ? 1u : 0u;
			const auto& c = config::player::stats::text_color_override[ i ];
			out.text_color_override[ i ]           =
				( static_cast< std::uint32_t >( c.r )       ) |
				( static_cast< std::uint32_t >( c.g ) <<  8 ) |
				( static_cast< std::uint32_t >( c.b ) << 16 ) |
				( static_cast< std::uint32_t >( c.a ) << 24 );
			out.text_outline_enabled[ i ]          = config::player::stats::text_outline_enabled[ i ] ? 1u : 0u;
			out.text_outline_width[ i ]            = config::player::stats::text_outline_width[ i ];
			out.text_background_enabled[ i ]       = config::player::stats::text_background_enabled[ i ] ? 1u : 0u;
			out.text_anchors[ i ]                  = config::player::stats::text_anchors[ i ];
			out.text_orders [ i ]                  = config::player::stats::text_orders [ i ];
		}
		return out;
	}

	static inline void apply_config( const config_snapshot_t& in ) {
		config::aimbot::enabled = in.aimbot_enabled;
		config::aimbot::curve = in.curve;
		config::aimbot::curve_behind_walls = in.curve_behind_walls;
		config::aimbot::bezier_curve = in.bezier_curve;
		config::aimbot::bspline_curve = in.bspline_curve;
		config::aimbot::chaikin_curve = in.chaikin_curve;
		config::aimbot::staggered_curve = in.staggered_curve;
		config::aimbot::aim_at_npcs = in.aim_at_npcs;
		config::aimbot::aim_at_knocked = in.aim_at_knocked;
		config::aimbot::sticky_target = in.sticky_target;
		config::aimbot::field_of_view = in.field_of_view;
		config::aimbot::curve_noise = in.curve_noise;
		config::aimbot::deadzone = in.deadzone;
		config::aimbot::smoothing = in.smoothing;
		config::aimbot::acceleration = in.acceleration;
		config::aimbot::acceleration_threshold = in.acceleration_threshold;
		config::aimbot::prediction_type = in.prediction_type;
		config::aimbot::visuals::curve_line = in.curve_line;
		config::aimbot::visuals::field_of_view = in.fov_circle;
		config::aimbot::visuals::crosshair = in.crosshair;
		config::aimbot::visuals::fov_arrows = in.fov_arrows;
		config::aimbot::visuals::prediction_dot = in.prediction_dot;
		config::aimbot::visuals::threat_warning = in.threat_warning;
		config::aimbot::visuals::threat_distance = in.threat_distance;
		config::aimbot::visuals::threat_fov = in.threat_fov;
		config::triggerbot::enabled = in.triggerbot_enabled;
		config::triggerbot::visible_only = in.triggerbot_visible_only;
		config::triggerbot::fire_without_aim = in.triggerbot_fire_without_aim;
		config::triggerbot::deadzone_px = in.triggerbot_deadzone_px;
		config::triggerbot::max_distance_m = in.triggerbot_max_distance_m;
		config::triggerbot::pre_fire_ms = in.triggerbot_pre_fire_ms;
		config::triggerbot::post_fire_ms = in.triggerbot_post_fire_ms;
		config::triggerbot::weapon_mask = in.triggerbot_weapon_mask;

		config::player::box::normal = in.box_normal;
		config::player::box::corner = in.box_corner;
		config::player::box::filled = in.box_filled;
		config::player::box::outline = in.box_outline;
		config::player::skeleton::normal = in.skeleton_normal;
		config::player::skeleton::curved = in.skeleton_curved;
		config::player::skeleton::outline = in.skeleton_outline;
		config::player::skeleton::fingers = in.skeleton_fingers;
		config::player::skeleton::curve_smoothness = in.skeleton_curve_smoothness;
		config::player::snaplines::enabled = in.snaplines_enabled;
		config::player::snaplines::position = in.snaplines_position;
		config::player::snaplines::outline = in.snaplines_outline;
		config::player::stats::player_name = in.player_name;
		config::player::stats::battle_pass = in.battle_pass;
		config::player::stats::kill_deaths = in.kill_deaths;
		config::player::stats::winrate = in.winrate;
		config::player::stats::rank = in.rank;
		config::player::stats::platform = in.platform;
		config::player::stats::team_marker = in.team_marker;
		config::player::stats::streamsnipe = in.streamsnipe;
		config::player::stats::health_bar = in.health_bar;
		config::player::stats::hit_markers = in.hit_markers;
		config::player::stats::state = in.building;
		config::player::stats::distance = in.distance;
		config::player::stats::inventory = in.inventory;
		config::player::stats::weapon_name = in.weapon_name;
		config::player::stats::weapon_damage = in.weapon_damage;
		config::player::stats::weapon_ammo = in.weapon_ammo;
		config::loot::pickup = in.loot_pickup;
		config::loot::chest = in.loot_chest;
		config::loot::weakspot = in.loot_weakspot;
		config::loot::pickup_distance = in.loot_pickup_distance;
		config::loot::chest_distance = in.loot_chest_distance;
		config::loot::weakspot_distance = in.loot_weakspot_distance;
		config::aimbot::visible_only = in.visible_only;
		config::aimbot::aim_key = in.aim_key;
		config::aimbot::max_distance = in.max_distance;
		config::aimbot::target_priority = in.target_priority;
		config::player::stats::weapon_reloading    = in.weapon_reloading;
		config::player::snaplines::smoothing       = in.snaplines_smoothing;
		config::player::stats::health_bar_position = in.health_bar_position;
		config::player::stats::shield_bar_position = in.shield_bar_position;
		for ( int i = 0; i < config::player::stats::slot_count; ++i ) {
			config::player::stats::text_sizes[ i ]                    = in.text_sizes[ i ];
			config::player::stats::text_color_override_enabled[ i ]   = in.text_color_override_enabled[ i ] != 0;
			const auto packed = in.text_color_override[ i ];
			config::player::stats::text_color_override[ i ] = {
				static_cast< std::uint8_t >(   packed         & 0xFFu ),
				static_cast< std::uint8_t >( ( packed >>  8 ) & 0xFFu ),
				static_cast< std::uint8_t >( ( packed >> 16 ) & 0xFFu ),
				static_cast< std::uint8_t >( ( packed >> 24 ) & 0xFFu ),
			};
			config::player::stats::text_outline_enabled[ i ]          = in.text_outline_enabled[ i ] != 0;
			config::player::stats::text_outline_width[ i ]            = in.text_outline_width[ i ];
			config::player::stats::text_background_enabled[ i ]       = in.text_background_enabled[ i ] != 0;
			config::player::stats::text_anchors[ i ]                  = in.text_anchors[ i ];
			config::player::stats::text_orders [ i ]                  = in.text_orders [ i ];
		}
	}

	static inline bool read_config_snapshot( const std::wstring& path, config_snapshot_t& snapshot ) {
		std::ifstream file( path, std::ios::binary | std::ios::ate );
		if ( !file )
			return false;

		const auto size = file.tellg( );

		// Peek magic + version so we know which suffix truncation applies.
		std::uint32_t magic = 0, version = 0;
		file.seekg( 0 );
		file.read( reinterpret_cast< char* >( &magic ), sizeof( magic ) );
		file.read( reinterpret_cast< char* >( &version ), sizeof( version ) );
		if ( magic != 0x43525331 ) return false;

		std::size_t bytes = 0;
		switch ( version ) {
		case 4:  bytes = config_snapshot_v4_size;  break;
		case 5:  bytes = config_snapshot_v5_size;  break;
		case 6:  bytes = config_snapshot_v6_size;  break;
		case 7:  bytes = config_snapshot_v7_size;  break;
		case 8:  bytes = config_snapshot_v8_size;  break;
		case 9:  bytes = config_snapshot_v9_size;  break;
		// v10 files on disk used 12-wide per-slot arrays; the current
		// struct is 13-wide. Degrade v10 to v9 layout so pre-v10 fields
		// still restore.
		case 10: bytes = config_snapshot_v9_size;  break;
		case 11: bytes = config_snapshot_v11_size; break;
		case 12: bytes = sizeof( config_snapshot_t ); break;
		default: return false;
		}
		if ( size < static_cast< std::streamoff >( bytes ) )
			return false;

		snapshot = config_snapshot_t{};
		file.seekg( 0 );
		file.read( reinterpret_cast< char* >( &snapshot ), static_cast< std::streamsize >( bytes ) );
		return file.gcount( ) == static_cast< std::streamsize >( bytes )
		    && snapshot.magic == 0x43525331
		    && snapshot.version == version;
	}

	static inline bool save_config( ) {
		CreateDirectoryW( L"C:\\crisis", nullptr );
		std::ofstream file( "C:\\crisis\\crisis-config.bin", std::ios::binary | std::ios::trunc );
		if ( !file )
			return false;
		const config_snapshot_t snapshot = capture_config( );
		file.write( reinterpret_cast< const char* >( &snapshot ), sizeof( snapshot ) );
		return file.good( );
	}

	static inline bool load_config( ) {
		config_snapshot_t snapshot {};
		if ( !read_config_snapshot( L"C:\\crisis\\crisis-config.bin", snapshot ) )
			return false;
		apply_config( snapshot );
		return true;
	}

	static inline void reset_config( ) {
		config::aimbot::enabled = true;
		config::aimbot::curve = true;
		config::aimbot::curve_behind_walls = true;
		config::aimbot::bezier_curve = true;
		config::aimbot::bspline_curve = true;
		config::aimbot::chaikin_curve = true;
		config::aimbot::staggered_curve = false;
		config::aimbot::aim_at_npcs = true;
		config::aimbot::aim_at_knocked = true;
		config::aimbot::sticky_target = false;
		config::aimbot::visible_only = false;
		config::aimbot::aim_key = VK_RBUTTON;
		config::aimbot::field_of_view = 45.0f;
		config::aimbot::curve_noise = 2.0f;
		config::aimbot::deadzone = 10.0f;
		config::aimbot::smoothing = 7.0f;
		config::aimbot::acceleration = 0.0f;
		config::aimbot::acceleration_threshold = 150.0f;
		config::aimbot::max_distance = 300.0f;
		config::aimbot::target_priority = 0;
		config::aimbot::prediction_type = 0;
		config::aimbot::visuals::curve_line = true;
		config::aimbot::visuals::field_of_view = true;
		config::aimbot::visuals::crosshair = false;
		config::aimbot::visuals::fov_arrows = false;
		config::aimbot::visuals::prediction_dot = true;
		config::aimbot::visuals::threat_warning = false;
		config::aimbot::visuals::threat_distance = 100.0f;
		config::aimbot::visuals::threat_fov = 60.0f;
		config::triggerbot::enabled = false;
		config::triggerbot::visible_only = true;
		config::triggerbot::fire_without_aim = false;
		config::triggerbot::deadzone_px = 15.0f;
		config::triggerbot::max_distance_m = 50.0f;
		config::triggerbot::pre_fire_ms = 50.0f;
		config::triggerbot::post_fire_ms = 80.0f;
		config::triggerbot::weapon_mask = ~std::uint64_t( 0 );
		config::player::box::normal = true;
		config::player::box::corner = false;
		config::player::box::filled = true;
		config::player::box::outline = true;
		config::player::skeleton::normal = true;
		config::player::skeleton::curved = false;
		config::player::skeleton::outline = true;
		config::player::skeleton::fingers = false;
		config::player::skeleton::curve_smoothness = 0.5f;
		config::player::snaplines::enabled = true;
		config::player::snaplines::position = 2;
		config::player::snaplines::outline = true;
		config::player::stats::player_name = true;
		config::player::stats::battle_pass = false;
		config::player::stats::kill_deaths = true;
		config::player::stats::winrate = false;
		config::player::stats::rank = true;
		config::player::stats::platform = true;
		config::player::stats::team_marker = true;
		config::player::stats::streamsnipe = true;
		config::player::stats::health_bar = true;
		config::player::stats::hit_markers = true;
		config::player::stats::state = true;
		config::player::stats::distance = true;
		config::player::stats::inventory = false;
		config::player::stats::weapon_name = true;
		config::player::stats::weapon_damage = true;
		config::player::stats::weapon_ammo = true;
		config::player::stats::weapon_reloading = false;
		config::player::snaplines::smoothing = 0.35f;
		config::player::stats::health_bar_position = 0;
		config::player::stats::shield_bar_position = 3;
		{
			const int default_sizes  [ config::player::stats::slot_count ] = { 2, 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1 };
			const int default_anchors[ config::player::stats::slot_count ] = { 0, 0, 0, 0, 0, 0, 0,   3, 1, 1, 1, 1,   3 };
			const int default_orders [ config::player::stats::slot_count ] = { 6, 3, 4, 5, 2, 1, 0,   1, 5, 0, 1, 4,   0 };
			for ( int i = 0; i < config::player::stats::slot_count; ++i ) {
				config::player::stats::text_sizes[ i ]                    = default_sizes[ i ];
				config::player::stats::text_color_override_enabled[ i ]   = false;
				config::player::stats::text_color_override[ i ]           = { 255, 255, 255, 255 };
				config::player::stats::text_outline_enabled[ i ]          = true;
				config::player::stats::text_outline_width[ i ]            = 1.f;
				config::player::stats::text_background_enabled[ i ]       = false;
				config::player::stats::text_anchors[ i ]                  = default_anchors[ i ];
				config::player::stats::text_orders [ i ]                  = default_orders [ i ];
			}
		}
		config::loot::pickup = true;
		config::loot::chest = true;
		config::loot::weakspot = true;
		config::loot::pickup_distance = 500.0f;
		config::loot::chest_distance = 500.0f;
		config::loot::weakspot_distance = 500.0f;
	}

	static inline void update_search_input( ) {
		for ( int vk = 0; vk < 256; ++vk ) {
			const bool down = ( GetAsyncKeyState( vk ) & 0x8000 ) != 0;
			const bool pressed = down && !s_search_key_down[ vk ];
			s_search_key_down[ vk ] = down;

			if ( !pressed || !s_search_focused )
				continue;
			if ( vk == VK_ESCAPE || vk == VK_RETURN ) {
				s_search_focused = false;
			}
			else if ( vk == VK_BACK ) {
				if ( !s_search_query.empty( ) )
					s_search_query.pop_back( );
			}
			else if ( vk == VK_SPACE && s_search_query.size( ) < 40 ) {
				s_search_query.push_back( L' ' );
			}
			else if ( vk >= 'A' && vk <= 'Z' && s_search_query.size( ) < 40 ) {
				s_search_query.push_back( static_cast< wchar_t >( vk - 'A' + L'a' ) );
			}
			else if ( vk >= '0' && vk <= '9' && s_search_query.size( ) < 40 ) {
				s_search_query.push_back( static_cast< wchar_t >( vk ) );
			}
		}
	}

	// ---- Drag state -------------------------------------------------------------
	static float s_pos_x = -1.0f;   // -1 = centre on first frame
	static float s_pos_y = -1.0f;
	static bool  s_dragging = false;
	static float s_drag_off_x = 0.0f;
	static float s_drag_off_y = 0.0f;
	static bool  s_lmb_prev = false;
	static bool  s_rmb_prev = false;
	static float s_feature_sidebar_progress = 1.0f;
	static float s_tab_content_progress = 1.0f;
	static float s_last_animation_delta = 1.0f / 60.0f;
	static std::uint32_t s_animated_content_key = 0;
	static std::chrono::steady_clock::time_point s_last_animation_tick = std::chrono::steady_clock::now( );

	// ---- Helpers ----------------------------------------------------------------
	static inline bool hit_test( float mx, float my, float x, float y, float w, float h ) {
		return mx >= x && mx < x + w && my >= y && my < y + h;
	}

	// ---- Widget context ---------------------------------------------------------
	struct widget_ctx_t {
		float mx, my;
		bool  lmb, clicked;
		float animation_delta;
		bypass::overlay::color_t col_accent;
		bypass::overlay::color_t col_border;
		float clip_bottom = 1e30f;
	};

	static constexpr float wdg_gap = 8.0f;   // vertical gap between widgets

	// Checkbox — 14×14 rounded box, FA tick when checked, label right.
	// Advances y by row height + wdg_gap so the next widget stacks automatically.
	static inline void draw_checkbox(
		const widget_ctx_t& ctx, float bx, float& by,
		const wchar_t* label, bool* value )
	{
		constexpr float cb_row_h = 24.0f;
		if ( by + cb_row_h > ctx.clip_bottom ) return;
		constexpr float cb_sz = 16.0f;
		constexpr float cb_radius = 4.0f;
		constexpr float cb_gap = 9.0f;

		const float box_y = by + ( cb_row_h - cb_sz ) * 0.5f;
		auto& animation = s_checkbox_animations[ value ];
		if ( !animation.initialized ) {
			animation.progress = *value ? 1.0f : 0.0f;
			animation.initialized = true;
		}
		if ( ctx.clicked && hit_test( ctx.mx, ctx.my, bx, box_y, cb_sz, cb_sz ) ) {
			*value = !*value;
			animation.pulse = 1.0f;
		}

		const float target = *value ? 1.0f : 0.0f;
		const float checkbox_ease = 1.0f - std::exp( -ctx.animation_delta * 18.0f );
		animation.progress += ( target - animation.progress ) * checkbox_ease;
		animation.pulse *= std::exp( -ctx.animation_delta * 9.0f );
		if ( std::abs( target - animation.progress ) < 0.002f )
			animation.progress = target;
		const float progress = animation.progress * animation.progress * ( 3.0f - 2.0f * animation.progress );

		bypass::overlay::filled_rounded_rectangle(
			{ bx, box_y, cb_sz, cb_sz }, cb_radius,
			bypass::overlay::color_t { 22, 21, 40, 255 } );
		if ( progress < 0.999f )
			bypass::overlay::rounded_rectangle(
				{ bx, box_y, cb_sz, cb_sz }, cb_radius,
				bypass::overlay::color_t { ctx.col_border.r, ctx.col_border.g, ctx.col_border.b,
					static_cast< std::uint8_t >( ctx.col_border.a * ( 1.0f - progress ) ) }, 1.0f );

		if ( progress > 0.001f ) {
			const float accent_scale = 0.72f + progress * 0.28f + animation.pulse * 0.05f;
			const float accent_size = cb_sz * accent_scale;
			const float accent_offset = ( cb_sz - accent_size ) * 0.5f;
			bypass::overlay::filled_rounded_rectangle(
				{ bx + accent_offset, box_y + accent_offset, accent_size, accent_size },
				cb_radius * accent_scale,
				bypass::overlay::color_t { ctx.col_accent.r, ctx.col_accent.g, ctx.col_accent.b,
					static_cast< std::uint8_t >( 230.0f * progress ) } );

			const float check_scale = 0.72f + progress * 0.28f;
			const float check_size = ( cb_sz + 2.0f ) * check_scale;
			const float check_x = bx + cb_sz * 0.5f - check_size * 0.5f;
			const float check_y = box_y + cb_sz * 0.5f - check_size * 0.5f; (void)check_scale; (void)check_size; (void)check_x; (void)check_y; if(false)
			bypass::overlay::text_ex( L"",
				{ check_x, check_y, check_size, check_size },
				bypass::overlay::color_t { 255, 255, 255,
					static_cast< std::uint8_t >( 255.0f * progress ) }, 10.0f * check_scale,
				bypass::overlay::text_weight_t::regular,
				L"Font Awesome", bypass::overlay::text_align_t::center, false );
		}

		const bool cb_hovered = hit_test( ctx.mx, ctx.my, bx, by, cb_sz + cb_gap + 200.0f, cb_row_h );
		bypass::overlay::text_ex( label,
			{ bx + cb_sz + cb_gap, by, 200.0f, cb_row_h },
			cb_hovered
			? bypass::overlay::color_t { 255, 255, 255, 255 }
			: bypass::overlay::color_t { 255, 255, 255,
				static_cast< std::uint8_t >( 180.0f + 75.0f * progress ) },
			13.0f, bypass::overlay::text_weight_t::bold,
			L"Inter-Medium", bypass::overlay::text_align_t::left, false );

		by += cb_row_h + wdg_gap;
	}

	// Float slider — label + value above, filled track + growing knob below.
	// Advances y by row height + wdg_gap.
	static inline void draw_slider_float(
		const widget_ctx_t& ctx, float bx, float& by, float bw,
		const wchar_t* label, float* value, float v_min, float v_max )
	{
		constexpr float sl_lbl_sz = 13.0f;
		constexpr float sl_track_h = 5.0f;
		constexpr float sl_knob_r = 6.0f;
		constexpr float sl_knob_r_grabbed = 9.0f;
		constexpr float sl_row_h = sl_lbl_sz + 8.0f + sl_track_h;
		if ( by + sl_row_h > ctx.clip_bottom ) return;

		const float track_x = bx;
		const float track_y = by + sl_lbl_sz + 8.0f;
		const float knob_cy = track_y + sl_track_h * 0.5f;

		constexpr float drag_zone = 12.0f;
		const bool grabbed = ctx.lmb && hit_test( ctx.mx, ctx.my,
			track_x - drag_zone, track_y - drag_zone,
			bw + drag_zone * 2.0f, sl_track_h + drag_zone * 2.0f );

		if ( grabbed ) {
			const float raw = ( ctx.mx - track_x ) / bw;
			*value = v_min + ( raw < 0.0f ? 0.0f : raw > 1.0f ? 1.0f : raw ) * ( v_max - v_min );
		}

		const float target_t = ( std::clamp ) ( ( *value - v_min ) / ( v_max - v_min ), 0.0f, 1.0f );
		auto& animation = s_slider_animations[ value ];
		if ( !animation.initialized ) {
			animation.position = target_t;
			animation.initialized = true;
		}
		const float position_ease = 1.0f - std::exp(
			-ctx.animation_delta * ( grabbed ? 28.0f : 14.0f ) );
		const float grab_ease = 1.0f - std::exp( -ctx.animation_delta * 20.0f );
		animation.position += ( target_t - animation.position ) * position_ease;
		animation.grabbed += ( ( grabbed ? 1.0f : 0.0f ) - animation.grabbed ) * grab_ease;
		if ( std::abs( target_t - animation.position ) < 0.0005f )
			animation.position = target_t;

		const float visual_t = ( std::clamp ) ( animation.position, 0.0f, 1.0f );
		const float grab_progress = animation.grabbed * animation.grabbed * ( 3.0f - 2.0f * animation.grabbed );
		const float knob_cx = track_x + visual_t * bw;
		const float kr = sl_knob_r + ( sl_knob_r_grabbed - sl_knob_r ) * grab_progress;

		bypass::overlay::filled_rounded_rectangle(
			{ track_x, track_y, bw, sl_track_h }, sl_track_h * 0.5f,
			bypass::overlay::color_t { 22, 21, 40, 255 } );
		if ( visual_t > 0.0f )
			bypass::overlay::filled_rounded_rectangle(
				{ track_x, track_y, visual_t * bw, sl_track_h }, sl_track_h * 0.5f,
				bypass::overlay::color_t { ctx.col_accent.r, ctx.col_accent.g, ctx.col_accent.b, 200 } );

		const bool at_min = visual_t <= 0.001f;
		if ( !at_min ) {
			if ( grab_progress > 0.01f )
				bypass::overlay::filled_circle( { knob_cx, knob_cy }, kr + 4.0f,
					bypass::overlay::color_t { ctx.col_accent.r, ctx.col_accent.g, ctx.col_accent.b,
						static_cast< std::uint8_t >( 28.0f * grab_progress ) } );
			bypass::overlay::filled_circle( { knob_cx, knob_cy }, kr,
				bypass::overlay::color_t { ctx.col_accent.r, ctx.col_accent.g, ctx.col_accent.b, 255 } );
			bypass::overlay::filled_circle( { knob_cx, knob_cy }, kr - 2.5f,
				bypass::overlay::color_t { 255, 255, 255, 220 } );
		}

		const bool sl_hovered = hit_test( ctx.mx, ctx.my, track_x, by, bw, sl_row_h );
		bypass::overlay::text_ex( label,
			{ track_x, by, bw * 0.6f, sl_lbl_sz + 4.0f },
			sl_hovered
			? bypass::overlay::color_t { 255, 255, 255, 255 }
			: bypass::overlay::color_t { 255, 255, 255, 180 },
			sl_lbl_sz, bypass::overlay::text_weight_t::bold,
			L"Inter-Medium", bypass::overlay::text_align_t::left, false );

		const bool disabled_marker = ( v_min <= 0.0001f ) && ( *value <= v_min + 0.0001f );
		wchar_t val_buf[ 16 ];
		if ( disabled_marker )
			std::swprintf( val_buf, 16, L"disabled" );
		else
			std::swprintf( val_buf, 16, L"%.2f", *value );
		bypass::overlay::text_ex( val_buf,
			{ track_x, by, bw, sl_lbl_sz + 4.0f },
			bypass::overlay::color_t { ctx.col_accent.r, ctx.col_accent.g, ctx.col_accent.b, 220 }, sl_lbl_sz,
			bypass::overlay::text_weight_t::bold,
			L"Inter-Medium", bypass::overlay::text_align_t::right, false );

		by += sl_row_h + wdg_gap;
	}

	// Combo (dropdown) — label above, box + chevron, animated dropdown list.
	// Advances y by header height normally; by header + dropdown when open.
	static inline void draw_combo(
		widget_ctx_t& ctx, float bx, float& by, float bw,
		const wchar_t* label, const wchar_t* const* items, int item_count, int* selected )
	{
		constexpr float cb_lbl_sz = 13.0f;
		constexpr float cb_box_h = 24.0f;
		constexpr float cb_row_h = cb_lbl_sz + 6.0f + cb_box_h;
		if ( by + cb_row_h > ctx.clip_bottom ) return;
		constexpr float cb_item_h = 22.0f;
		constexpr float cb_pad = 8.0f;
		constexpr float cb_radius = 4.0f;

		auto& state = s_combo_states[ label ];

		const float box_y = by + cb_lbl_sz + 8.0f;

		// Toggle on header click; close on click outside the whole dropdown area
		const float dd_y = box_y + cb_box_h + 2.0f;
		const float dd_h = item_count * cb_item_h + cb_pad * 2.0f;
		if ( ctx.clicked ) {
			if ( hit_test( ctx.mx, ctx.my, bx, box_y, bw, cb_box_h ) )
				state.open = !state.open;
			else if ( state.open && !hit_test( ctx.mx, ctx.my, bx, dd_y, bw, dd_h ) )
				state.open = false;
		}

		// Label
		bypass::overlay::text_ex( label,
			{ bx, by, bw, cb_lbl_sz + 4.0f },
			bypass::overlay::color_t { 255, 255, 255, 180 }, cb_lbl_sz,
			bypass::overlay::text_weight_t::bold,
			L"Inter-Medium", bypass::overlay::text_align_t::left, false );

		// Box
		bypass::overlay::filled_rounded_rectangle(
			{ bx, box_y, bw, cb_box_h }, cb_radius,
			bypass::overlay::color_t { 22, 21, 40, 255 } );
		bypass::overlay::rounded_rectangle(
			{ bx, box_y, bw, cb_box_h }, cb_radius,
			state.open
			? bypass::overlay::color_t { ctx.col_accent.r, ctx.col_accent.g, ctx.col_accent.b, 200 }
			: ctx.col_border,
			1.0f );

		// Current value text
		const wchar_t* cur = ( *selected >= 0 && *selected < item_count ) ? items[ *selected ] : L"None";
		bypass::overlay::text_ex( cur,
			{ bx + cb_pad, box_y, bw - cb_pad * 2.0f - 18.0f, cb_box_h },
			bypass::overlay::color_t { 255, 255, 255, 200 }, 13.0f,
			bypass::overlay::text_weight_t::bold,
			L"Inter-Medium", bypass::overlay::text_align_t::left, false );

		// Chevron icon
		bypass::overlay::text_ex(
			state.open ? L"" : L"",
			{ bx + bw - 22.0f, box_y, 18.0f, cb_box_h },
			bypass::overlay::color_t { 255, 255, 255, 160 }, 10.0f,
			bypass::overlay::text_weight_t::regular,
			L"Font Awesome", bypass::overlay::text_align_t::center, false );

		if ( state.open ) {
			const bool dropdown_hovered = hit_test( ctx.mx, ctx.my, bx, dd_y, bw, dd_h );
			for ( int i = 0; i < item_count; ++i ) {
				const float item_y = dd_y + cb_pad + i * cb_item_h;
				const bool hov = hit_test( ctx.mx, ctx.my, bx, item_y, bw, cb_item_h );
				if ( ctx.clicked && hov ) {
					*selected = i;
					state.open = false;
				}
			}

			const int cur_sel = *selected;
			s_dropdown_draw_queue.push_back( [ = ]( ) {
				const auto found = s_combo_states.find( label );
				if ( found == s_combo_states.end( ) || !found->second.open ) return;
				bypass::overlay::filled_rounded_rectangle( { bx, dd_y, bw, dd_h }, cb_radius,
					bypass::overlay::color_t { 28, 27, 48, 252 } );
				bypass::overlay::rounded_rectangle( { bx, dd_y, bw, dd_h }, cb_radius, ctx.col_border, 1.0f );
				for ( int i = 0; i < item_count; ++i ) {
					const float item_y = dd_y + cb_pad + i * cb_item_h;
					const bool hov = hit_test( ctx.mx, ctx.my, bx, item_y, bw, cb_item_h );
					const bool sel = i == cur_sel;
					if ( hov ) bypass::overlay::filled_rounded_rectangle(
						{ bx + 4.0f, item_y + 1.0f, bw - 8.0f, cb_item_h - 2.0f }, 3.0f,
						bypass::overlay::color_t { ctx.col_accent.r, ctx.col_accent.g, ctx.col_accent.b, 40 } );
					bypass::overlay::text_ex( items[ i ], { bx + cb_pad, item_y, bw - cb_pad * 2.0f, cb_item_h },
						sel ? bypass::overlay::color_t { ctx.col_accent.r, ctx.col_accent.g, ctx.col_accent.b, 255 }
						: hov ? bypass::overlay::color_t { 255, 255, 255, 220 }
						: bypass::overlay::color_t { 255, 255, 255, 160 }, 13.0f,
						bypass::overlay::text_weight_t::bold, L"Inter-Medium", bypass::overlay::text_align_t::left, false );
				}
			} );
			if ( dropdown_hovered ) {
				ctx.clicked = false;
				ctx.lmb = false;
			}
		}
		by += cb_row_h + wdg_gap;
	}

	static inline void draw_multi_combo(
		widget_ctx_t& ctx, float bx, float& by, float bw,
		const wchar_t* label,
		const wchar_t* const* items, const int* bit_indices, int item_count,
		std::uint64_t* mask )
	{
		constexpr float cb_lbl_sz = 13.0f;
		constexpr float cb_box_h = 24.0f;
		constexpr float cb_row_h = cb_lbl_sz + 6.0f + cb_box_h;
		if ( by + cb_row_h > ctx.clip_bottom ) return;
		constexpr float cb_item_h = 22.0f;
		constexpr float cb_pad = 8.0f;
		constexpr float cb_radius = 4.0f;
		constexpr float tick_sz = 14.0f;

		auto& state = s_combo_states[ label ];

		const float box_y = by + cb_lbl_sz + 8.0f;
		const float dd_y = box_y + cb_box_h + 2.0f;
		const float dd_h = item_count * cb_item_h + cb_pad * 2.0f;

		if ( ctx.clicked ) {
			if ( hit_test( ctx.mx, ctx.my, bx, box_y, bw, cb_box_h ) )
				state.open = !state.open;
			else if ( state.open && !hit_test( ctx.mx, ctx.my, bx, dd_y, bw, dd_h ) )
				state.open = false;
		}

		bypass::overlay::text_ex( label,
			{ bx, by, bw, cb_lbl_sz + 4.0f },
			bypass::overlay::color_t { 255, 255, 255, 180 }, cb_lbl_sz,
			bypass::overlay::text_weight_t::bold,
			L"Inter-Medium", bypass::overlay::text_align_t::left, false );

		bypass::overlay::filled_rounded_rectangle(
			{ bx, box_y, bw, cb_box_h }, cb_radius,
			bypass::overlay::color_t { 22, 21, 40, 255 } );
		bypass::overlay::rounded_rectangle(
			{ bx, box_y, bw, cb_box_h }, cb_radius,
			state.open
			? bypass::overlay::color_t { ctx.col_accent.r, ctx.col_accent.g, ctx.col_accent.b, 200 }
			: ctx.col_border,
			1.0f );

		int sel_count = 0;
		for ( int i = 0; i < item_count; ++i )
			if ( *mask & ( std::uint64_t( 1 ) << bit_indices[ i ] ) )
				++sel_count;

		const std::wstring summary = sel_count == 0 ? L"None"
			: sel_count == item_count ? L"All"
			: std::to_wstring( sel_count ) + L" selected";
		bypass::overlay::text_ex( summary.c_str( ),
			{ bx + cb_pad, box_y, bw - cb_pad * 2.0f - 18.0f, cb_box_h },
			bypass::overlay::color_t { 255, 255, 255, 200 }, 13.0f,
			bypass::overlay::text_weight_t::bold,
			L"Inter-Medium", bypass::overlay::text_align_t::left, false );

		bypass::overlay::text_ex(
			state.open ? L"" : L"",
			{ bx + bw - 22.0f, box_y, 18.0f, cb_box_h },
			bypass::overlay::color_t { 255, 255, 255, 160 }, 10.0f,
			bypass::overlay::text_weight_t::regular,
			L"Font Awesome", bypass::overlay::text_align_t::center, false );

		if ( state.open ) {
			const bool dropdown_hovered = hit_test( ctx.mx, ctx.my, bx, dd_y, bw, dd_h );
			for ( int i = 0; i < item_count; ++i ) {
				const float item_y = dd_y + cb_pad + i * cb_item_h;
				const bool hov = hit_test( ctx.mx, ctx.my, bx, item_y, bw, cb_item_h );
				if ( ctx.clicked && hov )
					*mask ^= ( std::uint64_t( 1 ) << bit_indices[ i ] );
			}

				s_dropdown_draw_queue.push_back( [ = ]( ) {
				const auto found = s_combo_states.find( label );
				if ( found == s_combo_states.end( ) || !found->second.open ) return;
				bypass::overlay::filled_rounded_rectangle( { bx, dd_y, bw, dd_h }, cb_radius,
					bypass::overlay::color_t { 28, 27, 48, 252 } );
				bypass::overlay::rounded_rectangle( { bx, dd_y, bw, dd_h }, cb_radius, ctx.col_border, 1.0f );
				for ( int i = 0; i < item_count; ++i ) {
					const float item_y = dd_y + cb_pad + i * cb_item_h;
					const bool hov = hit_test( ctx.mx, ctx.my, bx, item_y, bw, cb_item_h );
					const bool sel = ( *mask & ( std::uint64_t( 1 ) << bit_indices[ i ] ) ) != 0;
					if ( hov ) bypass::overlay::filled_rounded_rectangle(
						{ bx + 4.0f, item_y + 1.0f, bw - 8.0f, cb_item_h - 2.0f }, 3.0f,
						bypass::overlay::color_t { ctx.col_accent.r, ctx.col_accent.g, ctx.col_accent.b, 40 } );
					const float tick_x = bx + bw - tick_sz - cb_pad;
					const float tick_y = item_y + ( cb_item_h - tick_sz ) * 0.5f;
					bypass::overlay::rounded_rectangle( { tick_x, tick_y, tick_sz, tick_sz }, 3.0f,
						sel ? bypass::overlay::color_t { ctx.col_accent.r, ctx.col_accent.g, ctx.col_accent.b, 255 }
						: bypass::overlay::color_t { 60, 58, 90, 255 }, 1.0f );
					bypass::overlay::text_ex( items[ i ], { bx + cb_pad, item_y, bw - cb_pad * 3.0f - tick_sz, cb_item_h },
						sel ? bypass::overlay::color_t { ctx.col_accent.r, ctx.col_accent.g, ctx.col_accent.b, 255 }
						: hov ? bypass::overlay::color_t { 255, 255, 255, 220 }
						: bypass::overlay::color_t { 255, 255, 255, 160 }, 13.0f,
						bypass::overlay::text_weight_t::bold, L"Inter-Medium", bypass::overlay::text_align_t::left, false );
				}
				} );
			if ( dropdown_hovered ) {
				ctx.clicked = false;
				ctx.lmb = false;
			}
		}
		by += cb_row_h + wdg_gap;
	}

	static const wchar_t* vk_name( int vk ) {
		switch ( vk ) {
		case 0x01: return L"M1";        case 0x02: return L"M2";
		case 0x04: return L"M3";        case 0x05: return L"M4";       case 0x06: return L"M5";
		case 0x08: return L"Backspace"; case 0x09: return L"Tab";       case 0x0D: return L"Enter";
		case 0x10: return L"Shift";     case 0x11: return L"Ctrl";      case 0x12: return L"Alt";
		case 0x13: return L"Pause";     case 0x14: return L"CapsLock";  case 0x1B: return L"Escape";
		case 0x20: return L"Space";
		case 0x21: return L"PgUp";      case 0x22: return L"PgDn";
		case 0x23: return L"End";       case 0x24: return L"Home";
		case 0x25: return L"Left";      case 0x26: return L"Up";
		case 0x27: return L"Right";     case 0x28: return L"Down";
		case 0x2C: return L"PrtScr";    case 0x2D: return L"Insert";    case 0x2E: return L"Delete";
		case 0x30: return L"0"; case 0x31: return L"1"; case 0x32: return L"2"; case 0x33: return L"3";
		case 0x34: return L"4"; case 0x35: return L"5"; case 0x36: return L"6"; case 0x37: return L"7";
		case 0x38: return L"8"; case 0x39: return L"9";
		case 0x41: return L"A"; case 0x42: return L"B"; case 0x43: return L"C"; case 0x44: return L"D";
		case 0x45: return L"E"; case 0x46: return L"F"; case 0x47: return L"G"; case 0x48: return L"H";
		case 0x49: return L"I"; case 0x4A: return L"J"; case 0x4B: return L"K"; case 0x4C: return L"L";
		case 0x4D: return L"M"; case 0x4E: return L"N"; case 0x4F: return L"O"; case 0x50: return L"P";
		case 0x51: return L"Q"; case 0x52: return L"R"; case 0x53: return L"S"; case 0x54: return L"T";
		case 0x55: return L"U"; case 0x56: return L"V"; case 0x57: return L"W"; case 0x58: return L"X";
		case 0x59: return L"Y"; case 0x5A: return L"Z";
		case 0x60: return L"Num0"; case 0x61: return L"Num1"; case 0x62: return L"Num2";
		case 0x63: return L"Num3"; case 0x64: return L"Num4"; case 0x65: return L"Num5";
		case 0x66: return L"Num6"; case 0x67: return L"Num7"; case 0x68: return L"Num8";
		case 0x69: return L"Num9"; case 0x6A: return L"Num*"; case 0x6B: return L"Num+";
		case 0x6D: return L"Num-"; case 0x6E: return L"Num."; case 0x6F: return L"Num/";
		case 0x70: return L"F1";  case 0x71: return L"F2";  case 0x72: return L"F3";
		case 0x73: return L"F4";  case 0x74: return L"F5";  case 0x75: return L"F6";
		case 0x76: return L"F7";  case 0x77: return L"F8";  case 0x78: return L"F9";
		case 0x79: return L"F10"; case 0x7A: return L"F11"; case 0x7B: return L"F12";
		case 0xA0: return L"LShift"; case 0xA1: return L"RShift";
		case 0xA2: return L"LCtrl";  case 0xA3: return L"RCtrl";
		case 0xA4: return L"LAlt";   case 0xA5: return L"RAlt";
		default:   return nullptr;
		}
	}

	// Keybind — label left, pill right; click pill to capture next VK; Escape to clear.
	// Advances y by one row height + wdg_gap.
	static inline void draw_keybind(
		const widget_ctx_t& ctx, float bx, float& by, float bw,
		const wchar_t* label, int* key )
	{
		constexpr float kb_row_h = 24.0f;
		if ( by + kb_row_h > ctx.clip_bottom ) return;
		constexpr float kb_pill_w = 64.0f;
		constexpr float kb_radius = 4.0f;
		constexpr float kb_lbl_fs = 13.0f;
		constexpr float kb_val_fs = 11.0f;
		constexpr float kb_icon_w = 18.0f;

		auto& state = s_keybind_states[ label ];

		const float pill_x = bx + bw - kb_pill_w;

		// Release guard so the click that opened waiting doesn't also assign
		if ( state.click_guard && !ctx.lmb )
			state.click_guard = false;

		// Toggle waiting on pill click
		if ( ctx.clicked && hit_test( ctx.mx, ctx.my, pill_x, by, kb_pill_w, kb_row_h ) ) {
			if ( state.status != KB_WAITING ) {
				state.status = KB_WAITING;
				state.click_guard = true;
				*key = 0;
			}
			else {
				state.status = KB_NONE;
			}
		}

		// Capture loop — runs every frame while waiting and guard is clear
		if ( state.status == KB_WAITING && !state.click_guard ) {
			if ( GetAsyncKeyState( VK_ESCAPE ) & 0x8000 ) {
				*key = 0;
				state.status = KB_NONE;
			}
			else {
				const int mouse_vks[ ] = { 0x01, 0x02, 0x04, 0x05, 0x06 };
				for ( int mv : mouse_vks ) {
					if ( GetAsyncKeyState( mv ) & 0x8000 ) {
						*key = mv;  state.status = KB_ASSIGNED;  break;
					}
				}
				if ( state.status == KB_WAITING ) {
					for ( int i = 0x08; i <= 0xA5; ++i ) {
						if ( i == VK_ESCAPE ) continue;
						if ( GetAsyncKeyState( i ) & 0x8000 ) {
							*key = i;  state.status = KB_ASSIGNED;  break;
						}
					}
				}
			}
		}

		// Sync status from external writes to *key
		if ( state.status != KB_WAITING )
			state.status = ( *key != 0 ) ? KB_ASSIGNED : KB_NONE;

		const bool waiting = ( state.status == KB_WAITING );

		// Label
		bypass::overlay::text_ex( label,
			{ bx, by, bw - kb_pill_w - 8.0f, kb_row_h },
			bypass::overlay::color_t { 255, 255, 255, 180 }, kb_lbl_fs,
			bypass::overlay::text_weight_t::bold,
			L"Inter-Medium", bypass::overlay::text_align_t::left, false );

		// Pill background + border
		bypass::overlay::filled_rounded_rectangle(
			{ pill_x, by, kb_pill_w, kb_row_h }, kb_radius,
			bypass::overlay::color_t { 22, 21, 40, 255 } );
		bypass::overlay::rounded_rectangle(
			{ pill_x, by, kb_pill_w, kb_row_h }, kb_radius,
			waiting
			? bypass::overlay::color_t { ctx.col_accent.r, ctx.col_accent.g, ctx.col_accent.b, 220 }
			: ctx.col_border,
			1.0f );

		// Key name
		const wchar_t* key_str = waiting ? L"..." : ( *key ? vk_name( *key ) : L"None" );
		if ( !key_str ) key_str = L"?";
		bypass::overlay::text_ex( key_str,
			{ pill_x + 6.0f, by, kb_pill_w - kb_icon_w - 10.0f, kb_row_h },
			waiting
			? bypass::overlay::color_t { ctx.col_accent.r, ctx.col_accent.g, ctx.col_accent.b, 255 }
			: bypass::overlay::color_t { 255, 255, 255, 200 },
			kb_val_fs,
			bypass::overlay::text_weight_t::bold,
			L"Inter-Medium", bypass::overlay::text_align_t::left, false );

		// Separator
		bypass::overlay::filled_rectangle(
			{ pill_x + kb_pill_w - kb_icon_w - 1.0f, by + 4.0f, 1.0f, kb_row_h - 8.0f },
			bypass::overlay::color_t { 255, 255, 255, 30 } );

		// Keyboard icon (FA f11c)
		bypass::overlay::text_ex( L"",
			{ pill_x + kb_pill_w - kb_icon_w, by, kb_icon_w, kb_row_h },
			waiting
			? bypass::overlay::color_t { ctx.col_accent.r, ctx.col_accent.g, ctx.col_accent.b, 220 }
			: bypass::overlay::color_t { 255, 255, 255, 120 },
			10.0f, bypass::overlay::text_weight_t::regular,
			L"Font Awesome", bypass::overlay::text_align_t::center, false );

		by += kb_row_h + wdg_gap;
	}

	// ---- Rainbow theme helpers ------------------------------------------------
	inline bypass::overlay::color_t hsl_to_rgb( float h, float s, float l ) {
		const float c = ( 1.0f - std::abs( 2.0f * l - 1.0f ) ) * s;
		const float x = c * ( 1.0f - std::abs( std::fmod( h / 60.0f, 2.0f ) - 1.0f ) );
		const float m = l - c * 0.5f;
		float r, g, b;
		if ( h < 60.0f ) { r = c; g = x; b = 0; }
		else if ( h < 120.0f ) { r = x; g = c; b = 0; }
		else if ( h < 180.0f ) { r = 0; g = c; b = x; }
		else if ( h < 240.0f ) { r = 0; g = x; b = c; }
		else if ( h < 300.0f ) { r = x; g = 0; b = c; }
		else { r = c; g = 0; b = x; }
		return {
			static_cast< std::uint8_t >( ( r + m ) * 255.0f ),
			static_cast< std::uint8_t >( ( g + m ) * 255.0f ),
			static_cast< std::uint8_t >( ( b + m ) * 255.0f ), 255 };
	}

	inline bypass::overlay::color_t rainbow_accent( float offset_degrees = 0.0f ) {
		static const auto s_start = std::chrono::steady_clock::now( );
		const float elapsed = std::chrono::duration< float >(
			std::chrono::steady_clock::now( ) - s_start ).count( );
		const float hue = std::fmod( elapsed * 24.0f + offset_degrees, 360.0f );
		return hsl_to_rgb( hue, 0.78f, 0.66f );
	}

	static constexpr std::uint32_t rainbow_theme_index = 5;

	// -------------------------------------------------------------------
	bypass::overlay::color_t current_accent( ) {
		static constexpr bypass::overlay::color_t menu_themes[ ] = {
			{ 150, 90, 255, 255 }, { 70, 145, 255, 255 }, { 105, 205, 175, 255 },
			{ 240, 120, 130, 255 }, { 240, 165, 100, 255 },
		};
		if ( s_menu_theme == rainbow_theme_index )
			return rainbow_accent( );
		return menu_themes[ ( std::min ) ( s_menu_theme,
			static_cast< std::uint32_t >( std::size( menu_themes ) - 1 ) ) ];
	}

	bypass::overlay::color_t inactive_accent( ) {
		static constexpr bypass::overlay::color_t menu_themes[ ] = {
			{ 150, 90, 255, 255 }, { 70, 145, 255, 255 }, { 105, 205, 175, 255 },
			{ 240, 120, 130, 255 }, { 240, 165, 100, 255 },
		};

		if ( s_menu_theme == rainbow_theme_index )
			return rainbow_accent( 120.0f );

		if ( s_menu_theme == 0 ) return { 70,  145, 255, 255 };
		if ( s_menu_theme == 1 ) return { 150, 90,  255, 255 };
		if ( s_menu_theme == 2 ) return { 70,  145, 255, 255 };
		if ( s_menu_theme == 4 ) return { 220, 70,  70,  255 };

		const auto next = ( s_menu_theme + 1 ) % static_cast< std::uint32_t >( std::size( menu_themes ) );
		return menu_themes[ next ];
	}

	bypass::overlay::color_t pastel_accent( const bypass::overlay::color_t& c ) {
		constexpr float mix = 0.45f; // how pastel — higher = more washed
		const float avg = ( c.r + c.g + c.b ) / 3.0f;
		return {
			static_cast< std::uint8_t >( c.r * ( 1.0f - mix ) + avg * mix ),
			static_cast< std::uint8_t >( c.g * ( 1.0f - mix ) + avg * mix ),
			static_cast< std::uint8_t >( c.b * ( 1.0f - mix ) + avg * mix ),
			c.a
		};
	}

	static inline void render_locked_content_screen(
		const wchar_t* section_name, const bypass::overlay::rect_t& area,
		bypass::overlay::color_t col_accent, float fade = 1.0f ) {
		if ( !section_name || area.width <= 0.0f || area.height <= 0.0f )
			return;
		fade = ( std::clamp )( fade, 0.0f, 1.0f );
		if ( fade <= 0.001f )
			return;
		// Scale every alpha value below by the caller's fade factor so
		// the entire locked overlay eases in when entering the tab and
		// eases out when leaving. Solid rects have to be scaled too so
		// the backdrop doesn't pop in as a hard rectangle mid-fade.
		const auto fa = [ & ]( std::uint8_t a ) {
			return static_cast< std::uint8_t >( static_cast< float >( a ) * fade );
		};

		const auto clipped = bypass::overlay::push_clip( area );
		bypass::overlay::filled_rectangle( area,
			bypass::overlay::color_t { 24, 24, 29, fa( 226 ) } );
		bypass::overlay::rectangle( area,
			bypass::overlay::color_t { 132, 132, 142, fa( 95 ) }, 1.0f );

		const float icon_size = ( std::min )( area.width, area.height ) * 0.22f;
		const float center_x = area.x + area.width * 0.5f;
		const float group_y = area.y + area.height * 0.5f - icon_size * 0.72f;
		bypass::overlay::text_ex(
			L"\uf023",
			{ center_x - icon_size * 0.5f, group_y, icon_size, icon_size },
			bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, fa( 235 ) },
			icon_size, bypass::overlay::text_weight_t::regular,
			L"Font Awesome", bypass::overlay::text_align_t::center, false );

		const std::wstring title = std::wstring( section_name ) + L" locked!";
		const float title_y = group_y + icon_size + 13.0f;
		bypass::overlay::text_ex(
			title.c_str( ),
			{ area.x + 24.0f, title_y, area.width - 48.0f, 28.0f },
			bypass::overlay::color_t { 247, 247, 250, fa( 245 ) }, 18.0f,
			bypass::overlay::text_weight_t::black,
			L"Geist-Bold", bypass::overlay::text_align_t::center, false );
		bypass::overlay::text_ex(
			L"This section has not been released yet.",
			{ area.x + 24.0f, title_y + 31.0f, area.width - 48.0f, 22.0f },
			bypass::overlay::color_t { 224, 224, 230, fa( 170 ) }, 13.0f,
			bypass::overlay::text_weight_t::regular,
			L"Inter-Regular", bypass::overlay::text_align_t::center, false );

		if ( clipped )
			bypass::overlay::pop_clip( );
	}

	// ─────────────────────────────────────────────────────────────
	// WIDGET LAYOUT MODE — framework only
	// ─────────────────────────────────────────────────────────────
	// Registry for on-screen widgets that opt into the layout mode.
	// Each widget's own render pass registers its bounding rect +
	// anchor once per frame; render_widget_layout_overlay() reads
	// the registry to draw placeholders / drag handles / spacing
	// helpers over them.
	//
	// Multi-widget-in-same-position support: when two or more items
	// share an anchor (e.g. both Radar and Top Bar dock top-left),
	// the group_by_anchor() helper stacks them with dynamic spacing
	// — exactly the same shape used by the ESP-preview text reflow
	// (see menu.cxx around the preview_slots block): sort by order,
	// walk items in stacking direction, accumulate offset by each
	// item's dominant dimension + a gap. Callers get back the
	// final on-screen rect for their item without knowing how many
	// siblings share the anchor.
	//
	// All drawing is INTENTIONALLY stubbed for now — see the
	// TODO(widget-layout) comments below. State + geometry + the
	// grouping algorithm are wired so the visuals can be plugged
	// in without touching any of the framework plumbing.
	namespace widget_layout {

		enum class anchor_t : std::uint8_t {
			top_left,      top_center,    top_right,
			middle_left,   middle_center, middle_right,
			bottom_left,   bottom_center, bottom_right,
		};

		enum class stack_dir_t : std::uint8_t {
			vertical,      // stack items downward from anchor
			horizontal,    // stack items rightward from anchor
		};

		struct item_t {
			const wchar_t* id            = L"";      // stable identifier ("radar", "top_bar", ...)
			const wchar_t* display_name  = L"";      // "Radar", "Top Bar", "Notifications"
			anchor_t       anchor        = anchor_t::top_left;
			stack_dir_t    stack         = stack_dir_t::vertical;
			float          offset_x      = 0.f;      // fine-tune offset from anchor after stacking
			float          offset_y      = 0.f;
			float          size_w        = 0.f;      // measured widget size (from caller)
			float          size_h        = 0.f;
			int            order         = 0;        // stacking order within its anchor group
			bool           visible       = true;
		};

		// Per-frame registry. Cleared at the start of every render
		// pass; each widget re-registers itself with its current
		// rect + anchor + order. Cleared-and-refilled every frame
		// so widgets that go invisible naturally drop out of the
		// layout overlay without needing an explicit unregister.
		inline std::vector< item_t > g_items;
		inline std::mutex            g_items_mutex;

		// Group-by-anchor scratch buffer, rebuilt inside
		// resolve_rects() every frame. Not thread-safe — only the
		// render thread reads/writes.
		inline std::array< std::vector< std::size_t >, 9 > g_groups;

		inline void begin_frame( ) {
			std::lock_guard lock( g_items_mutex );
			g_items.clear( );
		}

		inline void register_widget( item_t item ) {
			std::lock_guard lock( g_items_mutex );
			g_items.push_back( std::move( item ) );
		}

		// Per-widget interaction state, keyed by the id pointer
		// value (stable string literals in practice). Carries the
		// hover/drag scale interpolation plus a drop animation
		// counter so the widget can smoothly ease into its new
		// anchor after a release.
		struct state_t {
			float scale         = 1.0f;   // interpolated hover/drag scale
			float target_scale  = 1.0f;   // 1.0 idle, 1.12 hover, 1.18 drag
			bool  drop_active   = false;  // drop animation currently playing
			float drop_from_x   = 0.f;
			float drop_from_y   = 0.f;
			float drop_from_w   = 0.f;
			float drop_from_h   = 0.f;
			float drop_from_scale = 1.0f;
			float drop_t        = 1.0f;   // 0 → 1
			float drop_duration = 0.32f;
		};
		inline std::unordered_map< const wchar_t*, state_t > g_states;

		// Drag state — only one widget can be dragged at a time.
		// g_drag_id == nullptr → no drag in progress.
		inline const wchar_t* g_drag_id     = nullptr;
		inline float          g_drag_grab_dx = 0.f;   // cursor→widget top-left offset at grab
		inline float          g_drag_grab_dy = 0.f;
		inline float          g_drag_from_x  = 0.f;   // remembered on release for drop anim
		inline float          g_drag_from_y  = 0.f;
		inline float          g_drag_from_w  = 0.f;
		inline float          g_drag_from_h  = 0.f;
		inline int            g_drag_hint    = -1;    // 0-7 = currently highlighted hint

		// 8 possible drop anchors, in the visual order the spec asks
		// for: top row (L/C/R), middle row (L/R skipping center),
		// bottom row (L/C/R). Middle-center is intentionally omitted
		// per the spec.
		inline constexpr anchor_t k_drag_hint_anchors[ 8 ] = {
			anchor_t::top_left,      anchor_t::top_center,    anchor_t::top_right,
			anchor_t::middle_left,                            anchor_t::middle_right,
			anchor_t::bottom_left,   anchor_t::bottom_center, anchor_t::bottom_right,
		};

		// Given the dragged widget's size + the screen dimensions,
		// return the on-screen top-left of the i-th anchor hint.
		inline void hint_position( int i, float w, float h, float sw, float sh,
			float& out_x, float& out_y ) {
			constexpr float pad = 12.f;
			const anchor_t a = k_drag_hint_anchors[ i ];
			const bool is_left   = a == anchor_t::top_left   || a == anchor_t::middle_left   || a == anchor_t::bottom_left;
			const bool is_center = a == anchor_t::top_center || a == anchor_t::bottom_center;
			const bool is_right  = a == anchor_t::top_right  || a == anchor_t::middle_right  || a == anchor_t::bottom_right;
			const bool is_top    = a == anchor_t::top_left    || a == anchor_t::top_center    || a == anchor_t::top_right;
			const bool is_mid_v  = a == anchor_t::middle_left || a == anchor_t::middle_right;
			const bool is_bottom = a == anchor_t::bottom_left || a == anchor_t::bottom_center || a == anchor_t::bottom_right;
			out_x = is_left ? pad
				: is_center ? ( sw - w ) * 0.5f
				: is_right  ? sw - pad - w : 0.f;
			out_y = is_top ? pad
				: is_mid_v ? ( sh - h ) * 0.5f
				: is_bottom ? sh - pad - h : 0.f;
		}

		// Compute (x, y) for each item after dynamic stacking.
		// Same algorithm the ESP-preview text stack uses: group by
		// anchor, sort by order, walk in stack direction while
		// accumulating cumulative offset. Called once per frame
		// by render_widget_layout_overlay() before any drawing.
		//
		// Returns a parallel vector to g_items — same indices —
		// with each entry's final on-screen top-left in .x/.y and
		// its width/height in .w/.h. `screen_w`/`screen_h` are the
		// overlay canvas dimensions (bypass::overlay::get_size).
		struct resolved_rect_t { float x, y, w, h; };
		inline std::vector< resolved_rect_t > resolve_rects( float screen_w, float screen_h ) {
			std::lock_guard lock( g_items_mutex );

			// Group indices by anchor bucket.
			for ( auto& g : g_groups ) g.clear( );
			for ( std::size_t i = 0; i < g_items.size( ); ++i ) {
				const auto a = static_cast< std::size_t >( g_items[ i ].anchor );
				if ( a < g_groups.size( ) )
					g_groups[ a ].push_back( i );
			}

			// Sort each group by order (ascending).
			for ( auto& g : g_groups ) {
				std::sort( g.begin( ), g.end( ),
					[ ]( std::size_t a, std::size_t b ) {
						return g_items[ a ].order < g_items[ b ].order;
					} );
			}

			// Screen-margin padding kept off the physical edge so
			// the accent border + gradient outline drawn later
			// don't clip through widget rects.
			constexpr float k_edge_pad = 12.f;
			constexpr float k_gap      = 6.f;

			std::vector< resolved_rect_t > out( g_items.size( ), resolved_rect_t { 0.f, 0.f, 0.f, 0.f } );

			// Per-anchor cursor. Each anchor's coordinate space
			// interprets "cursor" differently: vertical stacks
			// accumulate Y, horizontal stacks accumulate X.
			for ( std::size_t bucket = 0; bucket < g_groups.size( ); ++bucket ) {
				const auto anchor_v = static_cast< anchor_t >( bucket );
				const auto& indices = g_groups[ bucket ];
				float cursor = 0.f;
				for ( std::size_t idx : indices ) {
					const auto& it = g_items[ idx ];
					if ( !it.visible ) continue;

					float x = 0.f, y = 0.f;
					const bool is_left   = anchor_v == anchor_t::top_left   || anchor_v == anchor_t::middle_left   || anchor_v == anchor_t::bottom_left;
					const bool is_center = anchor_v == anchor_t::top_center || anchor_v == anchor_t::middle_center || anchor_v == anchor_t::bottom_center;
					const bool is_right  = anchor_v == anchor_t::top_right  || anchor_v == anchor_t::middle_right  || anchor_v == anchor_t::bottom_right;
					const bool is_top    = anchor_v == anchor_t::top_left    || anchor_v == anchor_t::top_center    || anchor_v == anchor_t::top_right;
					const bool is_mid_v  = anchor_v == anchor_t::middle_left || anchor_v == anchor_t::middle_center || anchor_v == anchor_t::middle_right;
					const bool is_bottom = anchor_v == anchor_t::bottom_left || anchor_v == anchor_t::bottom_center || anchor_v == anchor_t::bottom_right;

					if      ( is_left )   x = k_edge_pad;
					else if ( is_center ) x = ( screen_w - it.size_w ) * 0.5f;
					else if ( is_right )  x = screen_w - k_edge_pad - it.size_w;

					if      ( is_top )    y = k_edge_pad;
					else if ( is_mid_v )  y = ( screen_h - it.size_h ) * 0.5f;
					else if ( is_bottom ) y = screen_h - k_edge_pad - it.size_h;

					// Apply stack cursor along the item's stack
					// direction, then per-item fine-tune offset.
					if ( it.stack == stack_dir_t::vertical ) {
						// Bottom-anchored stacks grow UPWARD so the
						// first item sits closest to the bottom edge
						// and additional items push away from it.
						if ( is_bottom ) y -= cursor;
						else             y += cursor;
						cursor += it.size_h + k_gap;
					}
					else {
						// Right-anchored horizontal stacks grow LEFT
						// for the same reason.
						if ( is_right ) x -= cursor;
						else            x += cursor;
						cursor += it.size_w + k_gap;
					}

					x += it.offset_x;
					y += it.offset_y;

					out[ idx ] = { x, y, it.size_w, it.size_h };
				}
			}
			return out;
		}

	} // namespace widget_layout

	// Framework entry point. Called from menu::render() when
	// s_widget_layout_mode is true, AFTER the menu panel and
	// notifications are rendered so this sits on top. Every draw
	// call is intentionally stubbed — the geometry, registry, and
	// dynamic spacing are all wired so the visuals can be dropped
	// in without touching any framework plumbing.
	static inline void render_widget_layout_overlay(
		float mx, float my, bool lmb, bool clicked,
		bypass::overlay::color_t col_accent,
		bypass::overlay::color_t col_border ) {
		( void ) col_border;

		// Screen canvas — every layout coordinate below is in
		// screen space. gui::screen_width / gui::screen_height
		// already reflect the active monitor's physical pixels.
		const float sw = static_cast< float >( gui::screen_width );
		const float sh = static_cast< float >( gui::screen_height );
		const float dt = s_last_animation_delta;

		// ── Register built-in widgets ────────────────────────
		// Widgets that have their own render paths (Radar, Top
		// Bar, ...) will register themselves from those paths
		// once the layout mode integration is wired there. For
		// now we register the notifications widget from inside
		// the overlay so the spec's "3-4 looping notification
		// examples inside an accent-outlined container" case
		// works end-to-end.
		widget_layout::begin_frame( );
		{
			widget_layout::item_t notif { };
			notif.id           = L"notifications";
			notif.display_name = L"Notifications";
			notif.anchor       = widget_layout::anchor_t::top_right;
			notif.stack        = widget_layout::stack_dir_t::vertical;
			// Container sized to comfortably fit 2-3 notification
			// cards. Card is 344 x 56 with 8 px gap; container
			// adds 8 px padding on every side.
			constexpr float k_card_w = 344.f;
			constexpr float k_card_h = 56.f;
			constexpr float k_card_gap = 8.f;
			constexpr float k_pad    = 8.f;
			notif.size_w = k_card_w + k_pad * 2.f;
			notif.size_h = k_card_h * 3.f + k_card_gap * 2.f + k_pad * 2.f;
			widget_layout::register_widget( notif );
		}

		// Resolve on-screen rects via the shared group/stack
		// algorithm (same shape ESP preview uses).
		auto resolved = widget_layout::resolve_rects( sw, sh );

		// ── Full-screen accent border ────────────────────────
		// A single 1.5 px accent-tinted rectangle around the
		// entire physical screen so the boundary of the drag
		// area is unambiguous even before you interact with
		// anything.
		bypass::overlay::rectangle(
			{ 0.5f, 0.5f, sw - 1.0f, sh - 1.0f },
			bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 220u },
			1.5f );

		// ── Per-widget interaction pass ──────────────────────
		// Two passes: (1) update hover / drop-anim / drag state
		// and compute each item's current draw rect, (2) draw
		// bg + outline + inner content. Split so hover reads
		// don't race with per-item draw ordering.
		std::vector< bypass::overlay::rect_t > draw_rects( resolved.size( ) );
		std::vector< float >                   draw_scales( resolved.size( ), 1.0f );
		std::size_t hovered_idx = static_cast< std::size_t >( -1 );

		// Which widget the cursor is over — pick the topmost
		// hit in reverse registry order so later-registered
		// widgets win a tie (matches typical z-order).
		if ( widget_layout::g_drag_id == nullptr ) {
			for ( std::size_t i = resolved.size( ); i-- > 0; ) {
				const auto& r = resolved[ i ];
				if ( r.w <= 0.f || r.h <= 0.f ) continue;
				if ( hit_test( mx, my, r.x, r.y, r.w, r.h ) ) {
					hovered_idx = i;
					break;
				}
			}
		}

		// Ease every state toward its target this frame.
		const float scale_ease = 1.f - std::exp( -dt / 0.08f );
		const float drop_step  = dt / 0.32f;
		for ( std::size_t i = 0; i < widget_layout::g_items.size( ); ++i ) {
			auto& it = widget_layout::g_items[ i ];
			auto& st = widget_layout::g_states[ it.id ];

			// Target scale: dragging → 1.18, hovering → 1.12,
			// otherwise 1.0. Applied uniformly so widgets look
			// like they lift off the surface on approach.
			const bool is_dragged = widget_layout::g_drag_id == it.id;
			st.target_scale = is_dragged ? 1.18f
				: ( hovered_idx == i ? 1.12f : 1.0f );
			st.scale += ( st.target_scale - st.scale ) * scale_ease;

			// Base rect from resolver; overridden for the
			// dragged widget (follows cursor) and for widgets
			// currently playing a drop animation.
			const auto& base = resolved[ i ];
			float rx = base.x, ry = base.y, rw = base.w, rh = base.h;
			if ( is_dragged ) {
				rx = mx - widget_layout::g_drag_grab_dx;
				ry = my - widget_layout::g_drag_grab_dy;
			}
			if ( st.drop_active ) {
				st.drop_t += drop_step;
				if ( st.drop_t >= 1.f ) {
					st.drop_t = 1.f;
					st.drop_active = false;
				}
				// Smoothstep for organic settle.
				const float t = st.drop_t;
				const float e = t * t * ( 3.f - 2.f * t );
				rx = st.drop_from_x + ( base.x - st.drop_from_x ) * e;
				ry = st.drop_from_y + ( base.y - st.drop_from_y ) * e;
				rw = st.drop_from_w + ( base.w - st.drop_from_w ) * e;
				rh = st.drop_from_h + ( base.h - st.drop_from_h ) * e;
				// Ride the drop-scale down over the same easing.
				st.scale = st.drop_from_scale
					+ ( 1.0f - st.drop_from_scale ) * e;
			}

			draw_rects[ i ]  = { rx, ry, rw, rh };
			draw_scales[ i ] = st.scale;
		}

		// Drag start — mousedown over a widget while not
		// already dragging.
		if ( clicked && hovered_idx != static_cast< std::size_t >( -1 ) &&
		     widget_layout::g_drag_id == nullptr ) {
			const auto& it = widget_layout::g_items[ hovered_idx ];
			const auto& r  = resolved[ hovered_idx ];
			widget_layout::g_drag_id     = it.id;
			widget_layout::g_drag_grab_dx = mx - r.x;
			widget_layout::g_drag_grab_dy = my - r.y;
			widget_layout::g_drag_from_x = r.x;
			widget_layout::g_drag_from_y = r.y;
			widget_layout::g_drag_from_w = r.w;
			widget_layout::g_drag_from_h = r.h;
		}

		// Drag update — recompute nearest hint each frame so
		// the highlight tracks the cursor.
		widget_layout::g_drag_hint = -1;
		if ( widget_layout::g_drag_id != nullptr ) {
			// Find dragged widget entry.
			std::size_t di = 0;
			for ( ; di < widget_layout::g_items.size( ); ++di )
				if ( widget_layout::g_items[ di ].id == widget_layout::g_drag_id ) break;
			if ( di < widget_layout::g_items.size( ) ) {
				const float dw = widget_layout::g_items[ di ].size_w;
				const float dh = widget_layout::g_items[ di ].size_h;
				float best_d2 = 1e18f;
				for ( int i = 0; i < 8; ++i ) {
					float hx, hy;
					widget_layout::hint_position( i, dw, dh, sw, sh, hx, hy );
					const float cx = hx + dw * 0.5f;
					const float cy = hy + dh * 0.5f;
					const float d2 = ( mx - cx ) * ( mx - cx ) + ( my - cy ) * ( my - cy );
					if ( d2 < best_d2 ) {
						best_d2 = d2;
						widget_layout::g_drag_hint = i;
					}
				}
			}
		}

		// Drop — LMB released while dragging.
		if ( !lmb && widget_layout::g_drag_id != nullptr ) {
			std::size_t di = 0;
			for ( ; di < widget_layout::g_items.size( ); ++di )
				if ( widget_layout::g_items[ di ].id == widget_layout::g_drag_id ) break;
			if ( di < widget_layout::g_items.size( ) && widget_layout::g_drag_hint >= 0 ) {
				auto& it = widget_layout::g_items[ di ];
				it.anchor = widget_layout::k_drag_hint_anchors[ widget_layout::g_drag_hint ];
				// Kick off the drop animation from the current
				// drag position back to the anchor's resolved
				// rect. The resolver runs again next frame with
				// the new anchor so `base` there = target.
				auto& st = widget_layout::g_states[ it.id ];
				st.drop_from_x = mx - widget_layout::g_drag_grab_dx;
				st.drop_from_y = my - widget_layout::g_drag_grab_dy;
				st.drop_from_w = it.size_w;
				st.drop_from_h = it.size_h;
				st.drop_from_scale = st.scale;
				st.drop_t = 0.f;
				st.drop_active = true;
			}
			widget_layout::g_drag_id  = nullptr;
			widget_layout::g_drag_hint = -1;
		}

		// ── Drag hints (8 anchor targets) ────────────────────
		// While a widget is being dragged, paint 8 filled
		// rectangles at every valid drop anchor, each sized to
		// the DRAGGED widget so what you see is exactly what
		// you'll get. The hint under the cursor gets a bolder
		// accent tint + border so the drop target is obvious.
		if ( widget_layout::g_drag_id != nullptr ) {
			std::size_t di = 0;
			for ( ; di < widget_layout::g_items.size( ); ++di )
				if ( widget_layout::g_items[ di ].id == widget_layout::g_drag_id ) break;
			if ( di < widget_layout::g_items.size( ) ) {
				const float dw = widget_layout::g_items[ di ].size_w;
				const float dh = widget_layout::g_items[ di ].size_h;
				for ( int i = 0; i < 8; ++i ) {
					float hx, hy;
					widget_layout::hint_position( i, dw, dh, sw, sh, hx, hy );
					const bool is_target = ( i == widget_layout::g_drag_hint );
					const std::uint8_t fill_a  = is_target ? 60u : 24u;
					const std::uint8_t stroke_a = is_target ? 220u : 90u;
					bypass::overlay::filled_rounded_rectangle(
						{ hx, hy, dw, dh }, 8.0f,
						bypass::overlay::color_t {
							col_accent.r, col_accent.g, col_accent.b, fill_a } );
					bypass::overlay::rounded_rectangle(
						{ hx, hy, dw, dh }, 8.0f,
						bypass::overlay::color_t {
							col_accent.r, col_accent.g, col_accent.b, stroke_a },
						is_target ? 1.5f : 1.0f );
				}
			}
		}

		// ── Per-widget draw pass ─────────────────────────────
		// Dark-grey transparent bg + accent outline for every
		// visible widget, scaled about its center by the eased
		// hover/drag scale. The dragged widget is drawn LAST so
		// it lands on top of any hint rectangle it happens to
		// overlap.
		auto draw_widget = [ & ] ( std::size_t i ) {
			if ( i >= widget_layout::g_items.size( ) ) return;
			const auto& it   = widget_layout::g_items[ i ];
			if ( !it.visible ) return;
			const auto& r    = draw_rects[ i ];
			if ( r.width <= 0.f || r.height <= 0.f ) return;
			const float s    = draw_scales[ i ];
			// Uniform scale about the widget's center. bypass::
			// overlay::rect_t uses .width/.height (not .w/.h) —
			// same struct the whole overlay API expects, so
			// reading with the wrong names never compiles.
			const float cx   = r.x + r.width  * 0.5f;
			const float cy   = r.y + r.height * 0.5f;
			const float sw_  = r.width  * s;
			const float sh_  = r.height * s;
			const float sx   = cx - sw_ * 0.5f;
			const float sy   = cy - sh_ * 0.5f;

			// Dark grey semi-transparent background.
			bypass::overlay::filled_rounded_rectangle(
				{ sx, sy, sw_, sh_ }, 10.0f,
				bypass::overlay::color_t { 36, 34, 46, 190u } );

			// Notifications: draw 3 example cards inside the
			// container, clipped so they can't spill out. Cards
			// bob through a fake spawn→hold→exit loop so the
			// preview visibly animates like the real thing.
			if ( it.id != nullptr && wcscmp( it.id, L"notifications" ) == 0 ) {
				bypass::overlay::push_clip( { sx, sy, sw_, sh_ } );
				constexpr float k_card_gap = 8.f;
				const float pad = 8.f * s;
				const float card_w = sw_ - pad * 2.f;
				const float card_h = 56.f * s;
				const float slot_h = card_h + k_card_gap * s;
				const float loop_period = 6.0f;
				const float now_s = std::chrono::duration< float >(
					std::chrono::steady_clock::now( ).time_since_epoch( ) ).count( );
				const float phase = std::fmod( now_s, loop_period ) / loop_period;
				const float y_shift = phase * slot_h;

				static const wchar_t* example_titles[ ] = {
					L"Config saved",
					L"Casino unlocked",
					L"Config edited",
					L"Session started",
				};
				static const wchar_t* example_bodies[ ] = {
					L"Aim tweaks were saved.",
					L"Won Magic Bullet from a crate.",
					L"Default profile was updated.",
					L"Welcome back to the overlay.",
				};

				for ( int c = 0; c < 4; ++c ) {
					const float cy_top = sy + pad + static_cast< float >( c ) * slot_h - y_shift;
					// Card row bg + border
					bypass::overlay::filled_rounded_rectangle(
						{ sx + pad, cy_top, card_w, card_h }, 10.0f,
						bypass::overlay::color_t { 28, 25, 40, 220u } );
					bypass::overlay::rounded_rectangle(
						{ sx + pad, cy_top, card_w, card_h }, 10.0f,
						bypass::overlay::color_t {
							col_accent.r, col_accent.g, col_accent.b, 150u }, 1.0f );

					// Icon box on the left, accent-tinted.
					const float icon_box = card_h - 12.f * s;
					const float icon_x   = sx + pad + 6.f;
					const float icon_y   = cy_top + ( card_h - icon_box ) * 0.5f;
					bypass::overlay::filled_rounded_rectangle(
						{ icon_x, icon_y, icon_box, icon_box }, 8.0f,
						bypass::overlay::color_t {
							col_accent.r, col_accent.g, col_accent.b, 70u } );

					// Header + body text.
					const float text_x = icon_x + icon_box + 8.f;
					const float text_w = ( sx + pad + card_w ) - text_x - 8.f;
					bypass::overlay::text_ex( example_titles[ c % 4 ],
						{ text_x, cy_top + 6.f * s, text_w, 20.f * s },
						bypass::overlay::color_t { 255, 255, 255, 235u },
						12.0f * s,
						bypass::overlay::text_weight_t::bold,
						L"Geist-Bold", bypass::overlay::text_align_t::left, false );
					bypass::overlay::text_ex( example_bodies[ c % 4 ],
						{ text_x, cy_top + 24.f * s, text_w, 18.f * s },
						bypass::overlay::color_t { 210, 205, 225, 200u },
						10.5f * s,
						bypass::overlay::text_weight_t::regular,
						L"Inter-Regular", bypass::overlay::text_align_t::left, false );
				}
				bypass::overlay::pop_clip( );
			}
			else if ( it.display_name ) {
				// Generic widget → centre-label with its display
				// name so the user knows which widget they're
				// grabbing.
				bypass::overlay::text_ex( it.display_name,
					{ sx, sy, sw_, sh_ },
					bypass::overlay::color_t { 230, 226, 240, 235u },
					13.0f * s,
					bypass::overlay::text_weight_t::semi_bold,
					L"Geist-Medium", bypass::overlay::text_align_t::center, false );
			}

			// Accent outline last so it draws over the interior.
			bypass::overlay::rounded_rectangle(
				{ sx, sy, sw_, sh_ }, 10.0f,
				bypass::overlay::color_t {
					col_accent.r, col_accent.g, col_accent.b, 235u }, 1.5f );
		};

		// First non-dragged, then dragged, so drag lands on top.
		for ( std::size_t i = 0; i < widget_layout::g_items.size( ); ++i ) {
			if ( widget_layout::g_items[ i ].id == widget_layout::g_drag_id )
				continue;
			draw_widget( i );
		}
		for ( std::size_t i = 0; i < widget_layout::g_items.size( ); ++i ) {
			if ( widget_layout::g_items[ i ].id == widget_layout::g_drag_id )
				draw_widget( i );
		}
	}

	static inline void render_content(
		float content_x, float content_y, float content_w, float content_h,
		float mx, float my, bool lmb, bool clicked, bool right_clicked,
		bypass::overlay::color_t col_accent,
		bypass::overlay::color_t col_border,
		bypass::overlay::color_t col_text_active ) {
		// Child panels
		constexpr float cp_pad = 12.0f;
		constexpr float cp_gap = 10.0f;
		constexpr float cp_hdr_h = 48.0f;
		constexpr float cp_icon_box = 28.0f;
		constexpr float content_intro_h = 54.0f;
		const float     cp_w = ( content_w - cp_pad * 2.0f - cp_gap ) * 0.5f;

		const std::wstring greeting = std::wstring( L"Hello, " ) + s_profile_name;
		const float profile_offset = s_profile_image_registered ? 54.0f : 0.0f;
		if ( s_profile_image_registered ) {
			bypass::overlay::filled_circle( { content_x + cp_pad + 22.0f, content_y + 10.0f + 22.0f }, 22.0f + 4.0f,
				bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 210 } );
			bypass::overlay::filled_circle( { content_x + cp_pad + 22.0f, content_y + 10.0f + 22.0f }, 22.0f + 1.5f,
				bypass::overlay::color_t { 26, 23, 42, 255 } );
			bypass::overlay::image( profile_image_id,
				{ content_x + cp_pad, content_y + 10.0f, 44.0f, 44.0f },
				bypass::overlay::color_t { 255, 255, 255, 255 }, 1.0f, 22.0f );
		}

		bypass::overlay::text_ex(
			greeting.c_str( ),
			{ content_x + cp_pad + profile_offset, content_y + 11.0f, 280.0f - profile_offset, 24.0f },
			bypass::overlay::color_t { 247, 245, 250, 240 }, 18.0f,
			bypass::overlay::text_weight_t::black,
			L"Geist-Bold", bypass::overlay::text_align_t::left, false );
		bypass::overlay::text_ex(
			L"Welcome Back!",
			{ content_x + cp_pad + profile_offset, content_y + 35.0f, 220.0f - profile_offset, 18.0f },
			bypass::overlay::color_t { 208, 204, 218, 105 }, 12.0f,
			bypass::overlay::text_weight_t::regular,
			L"Inter-Regular", bypass::overlay::text_align_t::left, false );

		constexpr float header_search_w = 300.0f;
		constexpr float header_search_h = 37.0f;
		const float header_search_x = content_x + content_w - header_search_w - 18.0f;
		const float header_search_y = content_y + 12.0f;
		const bool header_search_hovered = hit_test(
			mx, my, header_search_x, header_search_y, header_search_w, header_search_h );
		if ( clicked && header_search_hovered ) {
			s_search_focused = true;
		}
		bypass::overlay::filled_rounded_rectangle(
			{ header_search_x, header_search_y, header_search_w, header_search_h }, 9.0f,
			bypass::overlay::color_t { 22, 19, 35, 135 } );
		bypass::overlay::rounded_rectangle(
			{ header_search_x, header_search_y, header_search_w, header_search_h }, 9.0f,
			col_border, 1.0f );
		bypass::overlay::text_ex(
			L"",
			{ header_search_x + 6.0f, header_search_y, 30.0f, header_search_h },
			bypass::overlay::color_t { 220, 216, 230, 115 }, 13.0f,
			bypass::overlay::text_weight_t::regular,
			L"Font Awesome", bypass::overlay::text_align_t::center, false );
		bypass::overlay::text_ex(
			s_search_query.empty( ) ? L"Search..." : s_search_query.c_str( ),
			{ header_search_x + 40.0f, header_search_y,
			  header_search_w - 52.0f, header_search_h },
			s_search_query.empty( )
			? bypass::overlay::color_t { 220, 216, 230, 100 }
			: bypass::overlay::color_t { 232, 228, 240, 205 },
			13.0f,
			bypass::overlay::text_weight_t::regular,
			L"Inter-Regular", bypass::overlay::text_align_t::left, false );

		const bool content_locked = s_sidebar_tab == 0 && s_sidebar_subtab == 1;
		const wchar_t* locked_section_name = content_locked ? L"Triggerbot" : nullptr;
		if ( content_locked ) {
			// Preserve the normal control rendering, but make the entire sub-tab
			// body inert before any widget or custom interaction code executes.
			lmb = false;
			clicked = false;
			right_clicked = false;
		}
		widget_ctx_t wctx { mx, my, lmb, clicked, s_last_animation_delta, col_accent, col_border };

		const bool body_fade_layer = s_tab_content_progress < 0.999f
			&& bypass::overlay::push_opacity_layer(
				{ content_x, content_y + content_intro_h, content_w, content_h - content_intro_h },
				0.70f + s_tab_content_progress * 0.30f );

		// Tab / subtab slide transition — combined (tab*8 + subtab)
		// index change bumps s_tab_slide_dir (+1 if moving down the
		// list of tabs, -1 if up), resets s_tab_slide_t to 0, then
		// ramps back to 1 each frame. draw_child adds
		// s_tab_slide_y = (1 − eased) × dir × 24px to _cy so the
		// incoming body slides in from the direction of motion.
		static int   s_tab_last_combined = -1;
		static float s_tab_slide_t       = 1.f;
		static float s_tab_slide_dir     = 0.f;
		static float s_tab_slide_y       = 0.f;
		static float s_tab_slide_fade    = 1.f;   // 0 at start of slide → 1 when settled
		{
			constexpr int   kMaxSub    = 8;
			constexpr float kSlideDist = 24.f;
			static auto s_slide_last = std::chrono::steady_clock::now( );
			const auto  now_hp = std::chrono::steady_clock::now( );
			const float dt = ( std::min )(
				std::chrono::duration< float >( now_hp - s_slide_last ).count( ),
				0.05f );
			s_slide_last = now_hp;

			const int cur_combined = static_cast< int >( s_sidebar_tab ) * kMaxSub
			                       + static_cast< int >( s_sidebar_subtab );
			if ( s_tab_last_combined < 0 ) {
				s_tab_last_combined = cur_combined;
				s_tab_slide_t       = 1.f;
				s_tab_slide_dir     = 0.f;
			}
			else if ( cur_combined != s_tab_last_combined ) {
				s_tab_slide_dir     = ( cur_combined > s_tab_last_combined ) ? 1.f : -1.f;
				s_tab_last_combined = cur_combined;
				s_tab_slide_t       = 0.f;
			}
			s_tab_slide_t += ( 1.f - s_tab_slide_t ) * ( 1.f - std::exp( -dt * 12.f ) );
			if ( 1.f - s_tab_slide_t < 0.002f ) s_tab_slide_t = 1.f;
			const float p     = ( std::clamp )( s_tab_slide_t, 0.f, 1.f );
			const float eased = p * p * ( 3.f - 2.f * p );   // smoothstep
			s_tab_slide_y    = ( 1.f - eased ) * s_tab_slide_dir * kSlideDist;
			s_tab_slide_fade = eased;   // reused by render_body / draw_child
		}

		// Core drawing function — explicit size so any layout can call it
		auto draw_child = [ & ]( float _cx, float _cy, float _w, float _h, child_t& c ) {
			_cy += s_tab_slide_y;   // tab-transition slide
			c.last_height = _h;
			bypass::overlay::filled_rounded_rectangle( { _cx, _cy, _w, _h }, 10.0f,
				bypass::overlay::color_t { 22, 19, 35, 135 } );
			bypass::overlay::rounded_rectangle( { _cx, _cy, _w, _h }, 10.0f, col_border, 1.0f );

			const float icon_x = _cx + 11.0f;
			const float icon_y = _cy + ( cp_hdr_h - cp_icon_box ) * 0.5f;
			bypass::overlay::text_ex( c.icon,
				{ icon_x, icon_y, cp_icon_box, cp_icon_box },
				col_accent, 16.0f, bypass::overlay::text_weight_t::regular,
				L"Font Awesome", bypass::overlay::text_align_t::center, false );

			const float text_x = icon_x + cp_icon_box + 4.0f;
			const float text_maxw = _w - ( text_x - _cx ) - cp_pad - 28.0f;
			bypass::overlay::text_ex( c.title,
				{ text_x, _cy, text_maxw, cp_hdr_h },
				bypass::overlay::color_t { 245, 243, 249, 235 }, 15.0f,
				bypass::overlay::text_weight_t::black,
				L"Geist-Bold", bypass::overlay::text_align_t::left, false );

			bypass::overlay::text_ex(
				L"",
				{ _cx + _w - cp_pad - 24.0f, _cy + ( cp_hdr_h - 24.0f ) * 0.5f,
				  24.0f, 24.0f },
				bypass::overlay::color_t { 225, 221, 234, 105 }, 11.0f,
				bypass::overlay::text_weight_t::regular,
				L"Font Awesome", bypass::overlay::text_align_t::center, false );

			constexpr float fade_edge_w = 12.0f;
			constexpr float fade_mid_w = 14.0f;
			const float divider_x = _cx + cp_pad;
			const float divider_y = _cy + cp_hdr_h;
			const float divider_w = _w - cp_pad * 2.0f;

			if ( divider_w > 1.0f ) {
				constexpr int   divider_segments = 64;
				constexpr float divider_edge_fade = 0.38f; // wider fade area
				constexpr float divider_max_alpha = 24.0f; // subtler center line

				for ( int i = 0; i < divider_segments; ++i ) {
					const float x0 = divider_x + divider_w * ( static_cast< float >( i ) / divider_segments );
					const float x1 = divider_x + divider_w * ( static_cast< float >( i + 1 ) / divider_segments );
					const float segment_w = x1 - x0;
					if ( segment_w <= 0.0f )
						continue;

					const float t = ( static_cast< float >( i ) + 0.5f ) / static_cast< float >( divider_segments );
					const float side = ( std::min ) ( t, 1.0f - t );

					float fade = side / divider_edge_fade;
					fade = ( std::clamp ) ( fade, 0.0f, 1.0f );

					// Smoothstep easing for a softer transition
					fade = fade * fade * ( 3.0f - 2.0f * fade );

					const auto alpha = static_cast< std::uint8_t >( divider_max_alpha * fade );
					if ( alpha == 0 )
						continue;

					bypass::overlay::filled_rectangle(
						{ x0, divider_y, segment_w, 1.0f },
						bypass::overlay::color_t { 255, 255, 255, alpha } );
				}
			}
			};

		auto render_children = [ & ]( child_t* arr, std::uint32_t count ) {
			const std::uint32_t total_rows = ( count + 1 ) / 2;
			const float row_h = ( content_h - content_intro_h - cp_pad * 2.0f
				- ( total_rows > 1 ? ( total_rows - 1 ) * cp_gap : 0.0f ) )
				/ static_cast< float >( total_rows );
			for ( std::uint32_t i = 0; i < count; ++i ) {
				const std::uint32_t row = i / 2;
				const std::uint32_t col = i % 2;
				const std::uint32_t row_items = ( row == ( count - 1 ) / 2 && count % 2 == 1 ) ? 1u : 2u;
				const float _w = row_items == 1 ? content_w - cp_pad * 2.0f : cp_w;
				const float _h = ( content_h - content_intro_h - cp_pad * 2.0f
					- ( total_rows > 1 ? ( total_rows - 1 ) * cp_gap : 0.0f ) ) / static_cast< float >( total_rows );
				const float _cx = content_x + cp_pad + col * ( cp_w + cp_gap );
				const float _cy = content_y + content_intro_h + cp_pad + row * ( row_h + cp_gap );
				draw_child( _cx, _cy, _w, _h, arr[ i ] );
			}
			};

		auto render_children_1l2r = [ & ]( child_t* arr ) {
			const float full_h = content_h - content_intro_h - cp_pad * 2.0f;
			const float half_h = ( full_h - cp_gap ) * 0.5f;
			const float left_x = content_x + cp_pad;
			const float right_x = content_x + cp_pad + cp_w + cp_gap;
			const float top_y = content_y + content_intro_h + cp_pad;
			draw_child( left_x, top_y, cp_w, full_h, arr[ 0 ] );
			draw_child( right_x, top_y, cp_w, half_h, arr[ 1 ] );
			draw_child( right_x, top_y + half_h + cp_gap, cp_w, half_h, arr[ 2 ] );
			};

		auto render_body = [ & ](
			float panel_x, float panel_y, float panel_width,
			child_t& child, auto&& render_widgets ) {
				const float saved_clip = wctx.clip_bottom;
				// Shift widgets AND clip line by the same tab-slide
				// offset the chrome (draw_child) uses, so the entire
				// child — background, header, divider, and every
				// widget — travels together during a tab switch.
				const float slide = s_tab_slide_y;
				wctx.clip_bottom = panel_y + slide + child.last_height;
				float widget_y = panel_y + slide + cp_hdr_h + 14.0f;
				render_widgets( panel_x + 11.0f, widget_y, panel_width - 22.0f );
				wctx.clip_bottom = saved_clip;

				// Fade-in mask — a rect the colour of the panel
				// backdrop drawn over the whole child (chrome +
				// widgets) at (1 - eased) alpha, so as the tab slides
				// down into place the entire child fades up from that
				// backdrop. Uses `filled_rounded_rectangle` so the
				// mask honours the same 10-px corner radius the child
				// chrome does; that keeps corners crisp during the
				// fade instead of showing a hard square edge.
				if ( s_tab_slide_fade < 0.999f ) {
					const auto mask_a = static_cast< std::uint8_t >(
						( 1.f - s_tab_slide_fade ) * 235.f );
					bypass::overlay::filled_rounded_rectangle(
						{ panel_x, panel_y + slide, panel_width, child.last_height },
						10.0f,
						bypass::overlay::color_t { 12, 10, 20, mask_a } );
				}
			};


		const float full_h = content_h - content_intro_h - cp_pad * 2.0f;
		const float half_h = ( full_h - cp_gap ) * 0.5f;
		const float left_x = content_x + cp_pad;
		const float right_x = left_x + cp_w + cp_gap;
		const float top_y = content_y + content_intro_h + cp_pad;

		if ( s_search_query.empty( ) ) {
			if ( s_sidebar_tab == 0 && s_sidebar_subtab == 0 ) {
				render_children_1l2r( aimbot_general_children );

				render_body( left_x, top_y, cp_w, aimbot_general_children[ 0 ],
					[ & ]( float x, float& y, float w ) {
						draw_checkbox( wctx, x, y, L"Enabled", &config::aimbot::enabled );
						draw_checkbox( wctx, x, y, L"Sticky target", &config::aimbot::sticky_target );
						draw_checkbox( wctx, x, y, L"Visible only", &config::aimbot::visible_only );
						draw_checkbox( wctx, x, y, L"Aim at NPCs", &config::aimbot::aim_at_npcs );
						draw_checkbox( wctx, x, y, L"Aim at knocked players", &config::aimbot::aim_at_knocked );
						static constexpr const wchar_t* prediction_options[ ] = { L"None", L"Velocity", L"Bullet" };
						draw_combo( wctx, x, y, w, L"Prediction", prediction_options, 3, &config::aimbot::prediction_type );
						draw_slider_float( wctx, x, y, w, L"Field of View", &config::aimbot::field_of_view, 1.0f, 180.0f );
						draw_slider_float( wctx, x, y, w, L"Deadzone", &config::aimbot::deadzone, 0.0f, 50.0f );
						draw_slider_float( wctx, x, y, w, L"Smoothing", &config::aimbot::smoothing, 1.0f, 20.0f );
						draw_slider_float( wctx, x, y, w, L"Acceleration", &config::aimbot::acceleration, 0.0f, 5.0f );
						draw_slider_float( wctx, x, y, w, L"Accel. Threshold", &config::aimbot::acceleration_threshold, 10.0f, 500.0f );
					} );

				render_body( right_x, top_y, cp_w, aimbot_general_children[ 1 ],
					[ & ]( float x, float& y, float w ) {
						draw_checkbox( wctx, x, y, L"Target line", &config::aimbot::visuals::curve_line );
						draw_checkbox( wctx, x, y, L"FOV circle", &config::aimbot::visuals::field_of_view );
						draw_checkbox( wctx, x, y, L"Crosshair", &config::aimbot::visuals::crosshair );
						draw_checkbox( wctx, x, y, L"FOV arrows", &config::aimbot::visuals::fov_arrows );
						draw_checkbox( wctx, x, y, L"Prediction dot", &config::aimbot::visuals::prediction_dot );
					} );

				render_body( right_x, top_y + half_h + cp_gap, cp_w, aimbot_general_children[ 2 ],
					[ & ]( float x, float& y, float w ) {
						draw_keybind( wctx, x, y, w, L"Aim Key", &config::aimbot::aim_key );
						static constexpr const wchar_t* priority_options[ ] = { L"Crosshair", L"Distance", L"Balanced" };
						draw_combo( wctx, x, y, w, L"Target priority", priority_options, 3, &config::aimbot::target_priority );
						draw_slider_float( wctx, x, y, w, L"Max distance", &config::aimbot::max_distance, 10.0f, 500.0f );
					} );
			}
			else if ( s_sidebar_tab == 0 && s_sidebar_subtab == 2 ) {
				draw_child( left_x, top_y, cp_w, full_h, aimbot_curve_children[ 0 ] );

				render_body( left_x, top_y, cp_w, aimbot_curve_children[ 0 ],
					[ & ]( float x, float& y, float w ) {
						draw_checkbox( wctx, x, y, L"Curve aiming", &config::aimbot::curve );
						draw_checkbox( wctx, x, y, L"Continue behind walls", &config::aimbot::curve_behind_walls );
						draw_slider_float( wctx, x, y, w, L"Curve noise", &config::aimbot::curve_noise, 0.0f, 10.0f );
					} );

				// Algorithm preview replaces the former Curve Types and Curve Tuning cards.
				bypass::overlay::filled_rounded_rectangle(
					{ right_x, top_y, cp_w, full_h }, 10.0f,
					bypass::overlay::color_t { 22, 19, 35, 135 } );
				bypass::overlay::rounded_rectangle(
					{ right_x, top_y, cp_w, full_h }, 10.0f, col_border, 1.0f );

				int selected_algorithm = s_custom_curve ? 3
					: config::aimbot::staggered_curve ? 2
					: config::aimbot::bezier_curve ? 0
					: config::aimbot::bspline_curve ? 1 : 0;
				static constexpr const wchar_t* algorithm_names[ ] = {
					L"Bezier", L"B-spline", L"Step", L"Custom"
				};

				bypass::overlay::text_ex(
					L"", { right_x + 12.0f, top_y + 11.0f, 30.0f, 30.0f },
					col_accent, 16.0f, bypass::overlay::text_weight_t::regular,
					L"Font Awesome", bypass::overlay::text_align_t::center, false );
				bypass::overlay::text_ex(
					L"Curve Preview", { right_x + 48.0f, top_y + 15.0f, cp_w - 62.0f, 20.0f },
					bypass::overlay::color_t { 255, 255, 255, 255 }, 15.0f, bypass::overlay::text_weight_t::black,
					L"Geist-Bold", bypass::overlay::text_align_t::left, false );
				bypass::overlay::text_ex(
					L"",
					{ right_x + cp_w - cp_pad - 24.0f, top_y + 12.0f, 24.0f, 24.0f },
					bypass::overlay::color_t { 225, 221, 234, 105 }, 11.0f,
					bypass::overlay::text_weight_t::regular,
					L"Font Awesome", bypass::overlay::text_align_t::center, false );

				const float selector_x = right_x + 12.0f;
				const float selector_y = top_y + 52.0f;
				constexpr float sel_h = 34.0f;
				constexpr float sel_gap = 10.0f;
				constexpr float sel_radius = 16.0f;
				constexpr float sel_inset = 4.0f;
				const float selector_w = ( cp_w - 24.0f - sel_gap * 3.0f ) / 4.0f;

				bypass::overlay::filled_rounded_rectangle(
					{ selector_x, selector_y, cp_w - 24.0f, sel_h }, sel_radius,
					bypass::overlay::color_t { 28, 26, 40, 230 } );

				static float s_sel_anim_x = -1.0f;
				const float sel_target_x = selector_x + static_cast< float >( selected_algorithm ) * ( selector_w + sel_gap );
				if ( s_sel_anim_x < 0.0f )
					s_sel_anim_x = sel_target_x;
				else
					s_sel_anim_x += ( sel_target_x - s_sel_anim_x ) * 0.18f;
				if ( std::abs( sel_target_x - s_sel_anim_x ) < 0.5f )
					s_sel_anim_x = sel_target_x;

				bypass::overlay::filled_rounded_rectangle(
					{ s_sel_anim_x, selector_y + 2.0f, selector_w, sel_h - 2.0f - sel_inset },
					sel_radius - sel_inset,
					bypass::overlay::color_t { 48, 46, 55, 120 } );

				for ( int i = 0; i < 4; ++i ) {
					const float sx = selector_x + static_cast< float >( i ) * ( selector_w + sel_gap );
					const bool hovered = hit_test( mx, my, sx, selector_y, selector_w, sel_h );
					const bool active = i == selected_algorithm;

					if ( clicked && hovered ) {
						selected_algorithm = i;
						config::aimbot::bezier_curve = i == 0;
						config::aimbot::bspline_curve = i == 1;
						config::aimbot::staggered_curve = i == 2;
						s_custom_curve = i == 3;
						config::aimbot::chaikin_curve = false;
					}

					bypass::overlay::text_ex(
						algorithm_names[ i ], { sx, selector_y, selector_w, sel_h },
						active
						? bypass::overlay::color_t { 255, 255, 255, 255 }
						: bypass::overlay::color_t { 220, 216, 228, static_cast< std::uint8_t >( hovered ? 175u : 105u ) },
						11.0f, bypass::overlay::text_weight_t::semi_bold,
						L"Geist-Medium", bypass::overlay::text_align_t::center, false );
				}

				const float graph_x = right_x + 22.0f;
				const float graph_y = selector_y + sel_h + 16.0f;
				const float graph_w = cp_w - 44.0f;
				const float graph_h = full_h - ( graph_y - top_y ) - 28.0f;
				bypass::overlay::filled_rounded_rectangle(
					{ graph_x, graph_y, graph_w, graph_h }, 9.0f,
					bypass::overlay::color_t { 14, 13, 27, 210 } );
				bypass::overlay::rounded_rectangle(
					{ graph_x, graph_y, graph_w, graph_h }, 9.0f,
					bypass::overlay::color_t { 62, 66, 84, 105 }, 1.0f );

				constexpr int grid_count = 4;
				for ( int i = 1; i < grid_count; ++i ) {
					const float gx = graph_x + graph_w * i / grid_count;
					const float gy = graph_y + graph_h * i / grid_count;
					bypass::overlay::line(
						{ gx, graph_y + 8.0f }, { gx, graph_y + graph_h - 8.0f },
						bypass::overlay::color_t { 115, 108, 135, 28 }, 1.0f );
					bypass::overlay::line(
						{ graph_x + 8.0f, gy }, { graph_x + graph_w - 8.0f, gy },
						bypass::overlay::color_t { 115, 108, 135, 28 }, 1.0f );
				}

				const float plot_x = graph_x + 18.0f;
				const float plot_y = graph_y + 18.0f;
				const float plot_w = graph_w - 36.0f;
				const float plot_h = graph_h - 36.0f;
				bypass::overlay::line(
					{ plot_x, plot_y + plot_h }, { plot_x + plot_w, plot_y },
					bypass::overlay::color_t { 165, 158, 180, 35 }, 1.0f );

				auto control_to_screen = [ & ]( const curve_point_t& point ) {
					return bypass::overlay::point_t {
						plot_x + point.x * plot_w,
						plot_y + point.y * plot_h
					};
					};
				if ( selected_algorithm == 3 ) {
					if ( !lmb )
						s_dragged_curve_point = -1;
					if ( right_clicked ) {
						for ( std::size_t i = 1; i + 1 < s_custom_curve_points.size( ); ++i ) {
							const auto handle = control_to_screen( s_custom_curve_points[ i ] );
							if ( hit_test( mx, my, handle.x - 10.0f, handle.y - 10.0f, 20.0f, 20.0f ) ) {
								s_custom_curve_points.erase( s_custom_curve_points.begin( ) + i );
								s_dragged_curve_point = -1;
								break;
							}
						}
					}
					if ( clicked && hit_test( mx, my, plot_x, plot_y, plot_w, plot_h ) ) {
						for ( std::size_t i = 1; i + 1 < s_custom_curve_points.size( ); ++i ) {
							const auto handle = control_to_screen( s_custom_curve_points[ i ] );
							if ( hit_test( mx, my, handle.x - 10.0f, handle.y - 10.0f, 20.0f, 20.0f ) ) {
								s_dragged_curve_point = static_cast< int >( i );
								break;
							}
						}
						if ( s_dragged_curve_point == -1 && s_custom_curve_points.size( ) < 12 ) {
							const float point_x = ( std::clamp ) ( ( mx - plot_x ) / plot_w, 0.025f, 0.975f );
							const float point_y = ( std::clamp ) ( ( my - plot_y ) / plot_h, 0.025f, 0.975f );
							const auto position = std::upper_bound( s_custom_curve_points.begin( ),
								s_custom_curve_points.end( ), point_x,
								[ ]( float x, const curve_point_t& point ) { return x < point.x; } );
							s_dragged_curve_point = static_cast< int >( position - s_custom_curve_points.begin( ) );
							s_custom_curve_points.insert( position, { point_x, point_y } );
						}
					}
					if ( lmb && s_dragged_curve_point >= 1
						&& s_dragged_curve_point + 1 < static_cast< int >( s_custom_curve_points.size( ) ) ) {
						auto& handle = s_custom_curve_points[ s_dragged_curve_point ];
						const float min_x = s_custom_curve_points[ s_dragged_curve_point - 1 ].x + 0.025f;
						const float max_x = s_custom_curve_points[ s_dragged_curve_point + 1 ].x - 0.025f;
						handle.x = ( std::clamp ) ( ( mx - plot_x ) / plot_w, min_x, max_x );
						handle.y = ( std::clamp ) ( ( my - plot_y ) / plot_h, 0.025f, 0.975f );
					}

					for ( std::size_t i = 0; i + 1 < s_custom_curve_points.size( ); ++i )
						bypass::overlay::line( control_to_screen( s_custom_curve_points[ i ] ),
							control_to_screen( s_custom_curve_points[ i + 1 ] ),
							bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 80 }, 1.0f );
					for ( std::size_t i = 1; i + 1 < s_custom_curve_points.size( ); ++i ) {
						const auto handle = control_to_screen( s_custom_curve_points[ i ] );
						bypass::overlay::filled_circle( handle, 6.5f,
							bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b,
								static_cast< std::uint8_t >( static_cast< int >( i ) == s_dragged_curve_point ? 255 : 225 ) } );
						bypass::overlay::filled_circle( handle, 3.0f,
							bypass::overlay::color_t { 255, 255, 255, 240 } );
					}
				}

				constexpr int curve_segments = 48;
				bypass::overlay::point_t previous { plot_x, plot_y + plot_h };
				for ( int i = 1; i <= curve_segments; ++i ) {
					const float t = static_cast< float >( i ) / curve_segments;
					bypass::overlay::point_t current {};
					if ( selected_algorithm == 3 ) {
						std::size_t right = 1;
						while ( right + 1 < s_custom_curve_points.size( ) && t > s_custom_curve_points[ right ].x )
							++right;
						const std::size_t left = right - 1;
						const float span = s_custom_curve_points[ right ].x - s_custom_curve_points[ left ].x;
						const float local_t = span > 0.0f
							? ( t - s_custom_curve_points[ left ].x ) / span : 0.0f;
						const float y0 = left ? s_custom_curve_points[ left - 1 ].y : s_custom_curve_points[ left ].y;
						const float y1 = s_custom_curve_points[ left ].y;
						const float y2 = s_custom_curve_points[ right ].y;
						const float y3 = right + 1 < s_custom_curve_points.size( )
							? s_custom_curve_points[ right + 1 ].y : s_custom_curve_points[ right ].y;
						const float y = 0.5f * ( ( 2.0f * y1 ) + ( -y0 + y2 ) * local_t
							+ ( 2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3 ) * local_t * local_t
							+ ( -y0 + 3.0f * y1 - 3.0f * y2 + y3 ) * local_t * local_t * local_t );
						current = { plot_x + plot_w * t, plot_y + plot_h * ( std::clamp ) ( y, 0.0f, 1.0f ) };
					}
					else {
						float value;
						if ( selected_algorithm == 2 ) {
							// Staggered: two smoothstep phases — first half covers X,
							// second half covers Y.  Plotted as a double S-curve.
							const float pt = t <= 0.5f ? t * 2.0f : ( t - 0.5f ) * 2.0f;
							const float sm = pt * pt * ( 3.0f - 2.0f * pt );
							value = t <= 0.5f ? sm * 0.5f : 0.5f + sm * 0.5f;
						}
						else {
							value = selected_algorithm == 0
								? t * t * ( 3.0f - 2.0f * t )
								: selected_algorithm == 1
								? std::pow( t, 1.65f )
								: 0.5f - 0.5f * std::cos( static_cast< float >( std::numbers::pi ) * t );
						}
						current = { plot_x + plot_w * t, plot_y + plot_h * ( 1.0f - value ) };
					}
					bypass::overlay::line( previous, current, col_accent, 2.5f );
					previous = current;
				}

				bypass::overlay::filled_circle(
					{ plot_x, plot_y + plot_h }, 4.0f, col_accent );
				bypass::overlay::filled_circle(
					{ plot_x + plot_w, plot_y }, 4.0f, col_accent );
			}
			else if ( s_sidebar_tab == 0 && s_sidebar_subtab == 1 ) {
				render_children( aimbot_trigger_children,
					static_cast< std::uint32_t >( std::size( aimbot_trigger_children ) ) );

				render_body( left_x, top_y, cp_w, aimbot_trigger_children[ 0 ],
					[ & ]( float x, float& y, float w ) {
						draw_checkbox( wctx, x, y, L"Enabled", &config::triggerbot::enabled );
						draw_checkbox( wctx, x, y, L"Visible only", &config::triggerbot::visible_only );
						draw_checkbox( wctx, x, y, L"Fire without aim", &config::triggerbot::fire_without_aim );
						draw_slider_float( wctx, x, y, w, L"Crosshair deadzone (px)", &config::triggerbot::deadzone_px, 1.0f, 60.0f );
						draw_slider_float( wctx, x, y, w, L"Max distance (m)", &config::triggerbot::max_distance_m, 1.0f, 500.0f );
						static constexpr const wchar_t* weapon_items[ ] = {
							L"DMR", L"Pistol", L"Shotgun", L"SMG", L"Rifle", L"Sniper",
							L"Machine Pistol", L"Grenade Launcher", L"Explosive Rifle",
							L"Crossbow", L"Assault Rifle", L"Tactical Shotgun", L"Sniper Rifle",
							L"LMG", L"Minigun", L"Trap", L"Thrown", L"Vehicle Weapon",
							L"AR Bull Pup", L"AR Forward Grip", L"AR Flintlock",
							L"Semi-Auto Sniper", L"AR Drum Gun", L"Rail Gun", L"Geometry Gun"
						};
						static constexpr int weapon_bits[ ] = {
							0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 18, 19, 20, 21, 22, 23, 24, 26, 28
						};
						draw_multi_combo( wctx, x, y, w, L"Weapons", weapon_items, weapon_bits, 25, &config::triggerbot::weapon_mask );
					} );

				render_body( right_x, top_y, cp_w, aimbot_trigger_children[ 1 ],
					[ & ]( float x, float& y, float w ) {
						draw_slider_float( wctx, x, y, w, L"Pre-fire delay (ms)", &config::triggerbot::pre_fire_ms, 0.0f, 300.0f );
						draw_slider_float( wctx, x, y, w, L"Fire cooldown (ms)", &config::triggerbot::post_fire_ms, 0.0f, 500.0f );
					} );
			}
			else if ( s_sidebar_tab == 1 && s_sidebar_subtab == 0 ) {
				// Layout 2026-08-26 (final): 2×2 grid, every cell the
				// same half-height. Boxes (top-left) / Snaplines
				// (top-right) / Skeleton (bottom-left) / reserved
				// (bottom-right, drops in with no math rework). ESP
				// Preview stays a persistent right-side element
				// outside the main panel.
				const float bot_row_y = top_y + half_h + cp_gap;
				draw_child( left_x,  top_y,     cp_w, half_h, visuals_children[ 0 ] );
				draw_child( left_x,  bot_row_y, cp_w, half_h, visuals_children[ 1 ] );
				draw_child( right_x, top_y,     cp_w, half_h, visuals_children[ 2 ] );
				draw_child( right_x, bot_row_y, cp_w, half_h, visuals_children[ 3 ] );

				static constexpr const wchar_t* box_display_options[ ] = { L"None", L"Normal", L"Corner" };
				render_body( left_x, top_y, cp_w, visuals_children[ 0 ],
					[ & ]( float x, float& y, float w ) {
						// Combo replaces the old Normal / Corner checkbox
						// pair. Both bools stay in config for backward
						// compatibility AND for player.cxx which still
						// checks them independently. The combo derives
						// its 3-way state from the bool pair each frame,
						// then writes both back on change.
						int box_display = config::player::box::corner ? 2
						                : config::player::box::normal ? 1
						                : 0;
						draw_combo( wctx, x, y, w, L"Box display",
							box_display_options, 3, &box_display );
						config::player::box::normal = ( box_display == 1 );
						config::player::box::corner = ( box_display == 2 );

						draw_checkbox( wctx, x, y, L"Filled box", &config::player::box::filled );
						draw_checkbox( wctx, x, y, L"Outline", &config::player::box::outline );
						draw_slider_float( wctx, x, y, w, L"Smoothness", &config::player::box::corner_smoothness, 0.0f, 1.0f );
					} );

				static constexpr const wchar_t* skel_display_options[ ] = { L"None", L"CPU", L"GPU" };
				render_body( left_x, bot_row_y, cp_w, visuals_children[ 1 ],
					[ & ]( float x, float& y, float w ) {
						// Skeleton display combo (2026-08-26) — replaces
						// the old Normal / Curved checkbox pair. Both
						// bools stay in config for backward compat AND
						// for player.cxx which still checks them
						// independently. The combo derives its 3-way
						// state from the mutually-exclusive bool pair
						// each frame, then writes both back on change.
						int skel_display = config::player::skeleton::curved ? 2
						                 : config::player::skeleton::normal ? 1
						                 : 0;
						draw_combo( wctx, x, y, w, L"Skeleton display",
							skel_display_options, 3, &skel_display );
						config::player::skeleton::normal = ( skel_display == 1 );
						config::player::skeleton::curved = ( skel_display == 2 );

						draw_checkbox( wctx, x, y, L"Fingers", &config::player::skeleton::fingers );
						// "Outline" (renamed from "Skeleton outline")
						// sits under Fingers so it reads as an option
						// that modifies whatever skeleton mode above
						// is active, not a separate primary toggle.
						draw_checkbox( wctx, x, y, L"Outline", &config::player::skeleton::outline );
						draw_slider_float( wctx, x, y, w, L"Smoothness", &config::player::skeleton::curve_smoothness, 0.0f, 1.0f );
					} );

				static constexpr const wchar_t* snap_positions_general[ ] = { L"Top", L"Middle", L"Bottom" };
				render_body( right_x, top_y, cp_w, visuals_children[ 2 ],
					[ & ]( float x, float& y, float w ) {
						draw_checkbox( wctx, x, y, L"Enabled", &config::player::snaplines::enabled );
						draw_checkbox( wctx, x, y, L"Outline", &config::player::snaplines::outline );
						draw_combo( wctx, x, y, w, L"Position", snap_positions_general, 3, &config::player::snaplines::position );
						draw_slider_float( wctx, x, y, w, L"Smoothness", &config::player::snaplines::smoothing, 0.0f, 1.0f );
					} );

				render_body( right_x, bot_row_y, cp_w, visuals_children[ 3 ],
					[ & ]( float x, float& y, float w ) {
						draw_checkbox( wctx, x, y, L"Enabled", &config::player::tracers::enabled );
						draw_checkbox( wctx, x, y, L"Outline", &config::player::tracers::outline );
						draw_slider_float( wctx, x, y, w, L"Fade time (ms)", &config::player::tracers::fade_ms, 200.0f, 5000.0f );
						draw_slider_float( wctx, x, y, w, L"Sample interval (ms)", &config::player::tracers::sample_interval_ms, 10.0f, 250.0f );
						draw_slider_float( wctx, x, y, w, L"Thickness", &config::player::tracers::thickness, 0.5f, 5.0f );
					} );
			}
			else if ( s_sidebar_tab == 1 && s_sidebar_subtab == 1 ) {
				// Layout 2026-08-26 rework:
				//   Left column: Player Information (full height).
				//   Right-top: Weapon Information (renamed, weapon-only
				//              widgets after Snaplines moved out).
				//   Right-bottom: World Information (new; holds threat
				//                 warning + sound alert + threat sliders).
				// Right column split 50/50 (weapon set + world set are
				// roughly the same size now).
				const float right_top_h = ( full_h - cp_gap ) * 0.5f;
				const float right_bot_h = full_h - cp_gap - right_top_h;
				const float right_bot_y = top_y + right_top_h + cp_gap;

				draw_child( left_x,  top_y,        cp_w, full_h,      visuals_info_children[ 0 ] );
				draw_child( right_x, top_y,        cp_w, right_top_h, visuals_info_children[ 2 ] );
				draw_child( right_x, right_bot_y,  cp_w, right_bot_h, visuals_info_children[ 1 ] );

				render_body( left_x, top_y, cp_w, visuals_info_children[ 0 ],
					[ & ]( float x, float& y, float w ) {
						draw_checkbox( wctx, x, y, L"Player name", &config::player::stats::player_name );
						draw_checkbox( wctx, x, y, L"Battle pass", &config::player::stats::battle_pass );
						draw_checkbox( wctx, x, y, L"Kills / deaths", &config::player::stats::kill_deaths );
						draw_checkbox( wctx, x, y, L"Win rate", &config::player::stats::winrate );
						draw_checkbox( wctx, x, y, L"Rank", &config::player::stats::rank );
						draw_checkbox( wctx, x, y, L"Platform", &config::player::stats::platform );
						draw_checkbox( wctx, x, y, L"Team marker", &config::player::stats::team_marker );
						draw_checkbox( wctx, x, y, L"Streamsnipe", &config::player::stats::streamsnipe );
						draw_checkbox( wctx, x, y, L"Health bar", &config::player::stats::health_bar );
						draw_checkbox( wctx, x, y, L"Hit markers", &config::player::stats::hit_markers );
						draw_checkbox( wctx, x, y, L"Player State", &config::player::stats::state );
						draw_checkbox( wctx, x, y, L"Distance", &config::player::stats::distance );
					} );

				// Right-top: Weapon Information — only the ammo /
				// damage / weapon-display widgets. Snaplines used to
				// sit here; it moved to the general subtab. Threat
				// warning moved OUT to the new World Information child
				// below.
				static constexpr const wchar_t* weapon_display_options[ ] = { L"None", L"Weapon Name", L"Inventory Bar" };
				render_body( right_x, right_bot_y, cp_w, visuals_info_children[ 1 ],
					[ & ]( float x, float& y, float w ) {
						// Weapon-display combo replaces the old
						// Inventory + Weapon-name checkbox pair; both
						// bools stay in config for backward compatibility
						// AND for the render code that still checks them
						// (see player.cxx). The combo derives its 3-way
						// state from the mutually-exclusive bool pair
						// each frame, then writes both back on change.
						int weapon_display = config::player::stats::inventory  ? 2
						                   : config::player::stats::weapon_name ? 1
						                   : 0;
						draw_combo( wctx, x, y, w, L"Weapon display",
							weapon_display_options, 3, &weapon_display );
						config::player::stats::inventory   = ( weapon_display == 2 );
						config::player::stats::weapon_name = ( weapon_display == 1 );

						draw_checkbox( wctx, x, y, L"Weapon ammo", &config::player::stats::weapon_ammo );
						draw_checkbox( wctx, x, y, L"Weapon damage", &config::player::stats::weapon_damage );
						draw_checkbox( wctx, x, y, L"Weapon reloading", &config::player::stats::weapon_reloading );
					} );

				// Right-bottom: World Information — threat warning +
				// its sub-sliders + the new sound alert (user pick,
				// 2026-08-26). Any future world-state labels (storm,
				// vehicles, environmental hazards) belong here too.
				render_body( right_x, top_y, cp_w, visuals_info_children[ 2 ],
					[ & ]( float x, float& y, float w ) {
						draw_checkbox( wctx, x, y, L"Threat warning", &config::aimbot::visuals::threat_warning );
						draw_checkbox( wctx, x, y, L"Aim indicator", &config::aimbot::visuals::aim_indicator );
						draw_slider_float( wctx, x, y, w, L"Aim indicator angle", &config::aimbot::visuals::aim_indicator_angle, 1.0f, 30.0f );
						draw_slider_float( wctx, x, y, w, L"Threat distance", &config::aimbot::visuals::threat_distance, 10.0f, 500.0f );
						draw_slider_float( wctx, x, y, w, L"Threat FOV", &config::aimbot::visuals::threat_fov, 10.0f, 90.0f );
					} );
			}
			else if ( s_sidebar_tab == 1 && s_sidebar_subtab == 2 ) {
				// 1-left-2-right: World ESP | Radar (top) / Bullet Tracers (bottom)
				render_children_1l2r( world_general_children );

				render_body( left_x, top_y, cp_w, world_general_children[ 0 ],
					[ & ]( float x, float& y, float w ) {
						draw_checkbox( wctx, x, y, L"Loot", &config::loot::pickup );
						if ( config::loot::pickup )
							draw_slider_float( wctx, x, y, w, L"Loot distance", &config::loot::pickup_distance, 50.0f, 2000.0f );
						draw_checkbox( wctx, x, y, L"Chests", &config::loot::chest );
						if ( config::loot::chest )
							draw_slider_float( wctx, x, y, w, L"Chest distance", &config::loot::chest_distance, 50.0f, 2000.0f );
						draw_checkbox( wctx, x, y, L"Weakspots", &config::loot::weakspot );
						if ( config::loot::weakspot )
							draw_slider_float( wctx, x, y, w, L"Weakspot distance", &config::loot::weakspot_distance, 50.0f, 2000.0f );
					} );

				// Right-bottom panel: Bullet Tracers
				render_body( right_x, top_y + half_h + cp_gap, cp_w, world_general_children[ 2 ],
					[ & ]( float x, float& y, float w ) {
						draw_checkbox( wctx, x, y, L"Enabled", &config::bullet_tracers::enabled );
						if ( config::bullet_tracers::enabled ) {
							draw_checkbox( wctx, x, y, L"Travel animation", &config::bullet_tracers::travel_anim );
							if ( config::bullet_tracers::travel_anim )
								draw_slider_float( wctx, x, y, w, L"Travel speed", &config::bullet_tracers::travel_speed, 0.3f, 5.0f );
							draw_checkbox( wctx, x, y, L"Fade out", &config::bullet_tracers::fade_out );
							if ( config::bullet_tracers::fade_out )
								draw_slider_float( wctx, x, y, w, L"Fade duration (s)", &config::bullet_tracers::fade_duration, 0.1f, 3.0f );
							draw_checkbox( wctx, x, y, L"Outline", &config::bullet_tracers::outline );
							draw_checkbox( wctx, x, y, L"Use accent colour", &config::bullet_tracers::use_accent );
							draw_slider_float( wctx, x, y, w, L"Thickness", &config::bullet_tracers::thickness, 0.5f, 5.0f );
							draw_slider_float( wctx, x, y, w, L"Lifetime (s)", &config::bullet_tracers::lifetime, 0.3f, 5.0f );
							draw_checkbox( wctx, x, y, L"Muzzle line", &config::bullet_tracers::muzzle_line );
						}
					} );

				render_body( right_x, top_y, cp_w, world_general_children[ 1 ],
					[ & ]( float x, float& y, float w ) {
						draw_checkbox( wctx, x, y, L"Radar", &config::widgets::radar );
						draw_checkbox( wctx, x, y, L"Bullet tracers", &config::widgets::radar_tracers );
						draw_slider_float( wctx, x, y, w, L"Radar range (m)",
							&config::widgets::radar_range, 20.0f, 500.0f );
						draw_slider_float( wctx, x, y, w, L"Radar size",
							&config::widgets::radar_size, 0.5f, 1.6f );
					} );
			}
			else if ( s_sidebar_tab == 2 && s_sidebar_subtab == 0 ) {
				// Exploits ▸ General. Single left card today — hosts every
				// weapon-side exploit toggle (Magic Bullet, No Spread + its
				// two tuning sliders). Right column left empty for now so
				// the layout can absorb more exploit cards later without a
				// shape churn.
				draw_child( left_x, top_y, cp_w, full_h, exploits_general_children[ 0 ] );

				render_body( left_x, top_y, cp_w, exploits_general_children[ 0 ],
					[ & ]( float x, float& y, float w ) {
						draw_checkbox( wctx, x, y, L"Magic Bullet", &config::exploits::magic_bullet );
						draw_checkbox( wctx, x, y, L"No Spread",    &config::exploits::no_spread );
						// Spread Value — target value written into every
						// spread slot when No Spread is on. 0.0 = zero
						// reticle bloom / perfect accuracy; higher values
						// let the shots retain a controlled amount of
						// spread (useful for staying under anti-cheat
						// heuristics that flag literally-zero spread).
						draw_slider_float( wctx, x, y, w, L"Spread Value",
							&config::exploits::spread_value, 0.0f, 5.0f );
						// Max Allowed Spread — cap gate. When > 0 the
						// walker only overrides slots whose decrypted
						// spread exceeds this value, so shots stay natural
						// up to the cap and only excessive blooms get
						// clamped. 0.0 = always override every frame.
						draw_slider_float( wctx, x, y, w, L"Max Allowed Spread",
							&config::exploits::spread_max, 0.0f, 10.0f );
					} );

				bypass::overlay::filled_rounded_rectangle(
					{ right_x, top_y, cp_w, full_h }, 10.0f,
					bypass::overlay::color_t { 22, 19, 35, 135 } );
				bypass::overlay::rounded_rectangle(
					{ right_x, top_y, cp_w, full_h }, 10.0f, col_border, 1.0f );
			}
		}
		else {
			// ---- Search results ---------------------------------------------------
			const float results_x = left_x;
			const float results_y = top_y;
			const float results_w = ( right_x + cp_w ) - left_x;
			const float results_h = full_h;

			bypass::overlay::filled_rounded_rectangle( { results_x, results_y, results_w, results_h }, 10.0f,
				bypass::overlay::color_t { 22, 19, 35, 135 } );
			bypass::overlay::rounded_rectangle( { results_x, results_y, results_w, results_h }, 10.0f, col_border, 1.0f );

			auto to_lower = [ ]( std::wstring s ) {
				for ( auto& c : s ) if ( c >= L'A' && c <= L'Z' ) c += 32;
				return s;
				};
			const std::wstring q = to_lower( s_search_query );

			struct search_entry_t { std::uint32_t tab; std::uint32_t sub; const wchar_t* title; const wchar_t* group; };
			std::array<search_entry_t, 16> matches {};
			std::uint32_t match_count = 0;
			for ( std::uint32_t t = 0; t < std::size( sidebar_tabs ) && match_count < matches.size( ); ++t ) {
				const sidebar_tab_t& tab = sidebar_tabs[ t ];
				for ( std::uint32_t s = 0; s < tab.sub_count && match_count < matches.size( ); ++s ) {
					const wchar_t* sub_label = tab.subs[ s ].label;
					const std::wstring combined = std::wstring( tab.label ) + L" " + sub_label;
					if ( to_lower( combined ).find( q ) != std::wstring::npos )
						matches[ match_count++ ] = { t, s, sub_label, tab.label };
				}
			}

			constexpr float row_h = 46.0f;
			constexpr float row_gap = 8.0f;
			constexpr float row_pad = 12.0f;
			constexpr float pin_box = 30.0f;

			if ( match_count == 0 ) {
				bypass::overlay::text_ex( L"No matches found",
					{ results_x, results_y, results_w, results_h },
					bypass::overlay::color_t { 200, 196, 210, 140 }, 13.0f,
					bypass::overlay::text_weight_t::regular,
					L"Inter-Regular", bypass::overlay::text_align_t::center, false );
			}
			else {
				float row_y = results_y + row_pad;
				for ( std::uint32_t i = 0; i < match_count; ++i ) {
					const search_entry_t& m = matches[ i ];
					if ( row_y + row_h > results_y + results_h - row_pad )
						break;

					const float row_x = results_x + row_pad;
					const float row_w = results_w - row_pad * 2.0f;
					const bool row_hovered = hit_test( mx, my, row_x, row_y, row_w, row_h );

					bypass::overlay::filled_rounded_rectangle( { row_x, row_y, row_w, row_h }, 8.0f,
						row_hovered
						? bypass::overlay::color_t { 27, 24, 45, 230 }
					: bypass::overlay::color_t { 255, 255, 255, 8 } );
					bypass::overlay::rounded_rectangle( { row_x, row_y, row_w, row_h }, 8.0f,
						row_hovered ? bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 115 } : col_border, 1.0f );

					const std::wstring combined = std::wstring( m.group ) + L" " + m.title;
					const bool exact_match = q == to_lower( m.title ) || q == to_lower( combined );
					if ( exact_match ) {
						bypass::overlay::filled_rounded_rectangle(
							{ row_x + pin_box + 6.0f, row_y + 6.0f, row_w - pin_box - 18.0f, row_h - 12.0f }, 6.0f,
							bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 42 } );
					}

					bypass::overlay::text_ex( L"",
						{ row_x + 12.0f, row_y, pin_box, row_h },
						row_hovered ? col_accent : bypass::overlay::color_t { 205, 200, 218, 150 }, 15.0f,
						bypass::overlay::text_weight_t::regular,
						L"Font Awesome", bypass::overlay::text_align_t::center, false );

					bypass::overlay::text_ex( m.title,
						{ row_x + pin_box + 12.0f, row_y + 8.0f, row_w - pin_box - 24.0f, 18.0f },
						bypass::overlay::color_t { 236, 232, 243, 235 }, 13.5f,
						bypass::overlay::text_weight_t::bold,
						L"Geist-Medium", bypass::overlay::text_align_t::left, false );
					bypass::overlay::text_ex( m.group,
						{ row_x + pin_box + 12.0f, row_y + 26.0f, row_w - pin_box - 24.0f, 16.0f },
						bypass::overlay::color_t { 205, 200, 218, 110 }, 11.0f,
						bypass::overlay::text_weight_t::regular,
						L"Inter-Regular", bypass::overlay::text_align_t::left, false );

					if ( clicked && row_hovered ) {
						s_sidebar_tab = m.tab;
						s_sidebar_subtab = m.sub;
						s_expanded_tabs |= ( 1u << m.tab );
						s_search_query.clear( );
						s_search_focused = false;
					}

					row_y += row_h + row_gap;
				}
			}
		}

		if ( body_fade_layer )
			bypass::overlay::pop_layer( );

		// Locked-screen fade — the overlay eases in when the
		// user clicks INTO a locked sub-tab (Triggerbot today) and
		// eases OUT when they click away. Keeps the overlay alive
		// for the leave transition by rendering while fade > ~0
		// and remembering the last locked section name so the
		// title glyphs don't pop mid-fade. Time constant matches
		// the tab-slide fade so both animations feel synchronous.
		static float s_locked_fade = 0.0f;
		static const wchar_t* s_last_locked_section = nullptr;
		const float locked_target = content_locked ? 1.0f : 0.0f;
		{
			const float dt_locked = s_last_animation_delta;
			const float tau = 0.16f;
			s_locked_fade += ( locked_target - s_locked_fade )
				* ( 1.0f - std::exp( -dt_locked / tau ) );
			if ( content_locked )
				s_last_locked_section = locked_section_name;
		}
		if ( s_locked_fade > 0.002f && s_last_locked_section ) {
			render_locked_content_screen(
				s_last_locked_section,
				{
					content_x + cp_pad,
					content_y + content_intro_h + cp_pad,
					cp_w * 2.0f + cp_gap,
					content_h - content_intro_h - cp_pad * 2.0f
				},
				col_accent,
				s_locked_fade );
		}
	}

	static inline void render_main_tab_content(
		float content_x, float content_y, float content_w,
		float mx, float my, bool lmb, bool clicked,
		bypass::overlay::color_t col_accent,
		bypass::overlay::color_t col_border,
		bypass::overlay::color_t col_text_active ) {
		constexpr float page_pad = 18.0f;
		const float page_x = content_x + page_pad;
		const float page_y = content_y + page_pad;
		const float page_w = content_w - page_pad * 2.0f;

		const wchar_t* page_title = s_main_tab == 1 ? L"Configuration" : L"Profile";
		const wchar_t* page_description = s_main_tab == 1
			? L"Browse and manage feature configuration groups"
			: L"View account and interface preferences";
		const wchar_t* page_icon = s_main_tab == 1 ? L"" : L"";
		if ( s_main_tab == 1 ) {
			const float grid_x = page_x;
			const float grid_y = page_y;
			const float grid_w = page_w;
			const float grid_h = 480.0f;

			// Configurations Card — anchored to the BOTTOM of the content area
			const float cfg_w = grid_w * 0.52f;
			const float cfg_h = grid_h * 0.65f;
			const float cfg_x = grid_x;
			const float cfg_y = grid_y + grid_h - cfg_h; // bottom-aligned

			// Performance Graph Card — right side, same height as config card
			const float perf_gap = 16.0f;
			const float perf_w = grid_w - cfg_w - perf_gap;
			const float perf_h = cfg_h;
			const float perf_x = cfg_x + cfg_w + perf_gap;
			const float perf_y = cfg_y;
			const float settings_x = perf_x;
			const float settings_y = grid_y;
			const float settings_w = perf_w;
			const float settings_h = perf_y - settings_y - perf_gap;
			const float menu_settings_x = cfg_x;
			const float menu_settings_y = grid_y;
			const float menu_settings_w = cfg_w;
			const float menu_settings_h = cfg_y - grid_y - perf_gap;

			// --- Configurations Card State ---
			struct cfg_entry_t { std::wstring name; std::wstring date; std::wstring tags; std::uint64_t time; };
			static std::vector<cfg_entry_t> s_config_profiles;
			static bool s_profiles_scanned = false;
			static int s_selected_profile = -1;

			// Inline Creation State
			static bool s_is_creating = false;
			static std::wstring s_create_name = L"";
			static std::wstring s_create_tags = L"";
			static bool s_create_name_focused = true;
			static bool s_create_tags_focused = false;
			static std::array<bool, 256> s_create_keys {};

			// Edit / Rename State
			static bool s_is_editing = false;
			static std::wstring s_edit_name = L"";
			static std::wstring s_edit_tags = L"";
			static bool s_edit_name_focused = true;
			static bool s_edit_tags_focused = false;
			static int s_editing_index = -1;
			static std::array<bool, 256> s_edit_keys {};

			static std::wstring s_cfg_search = L"";
			static bool s_cfg_search_focused = false;
			static std::array<bool, 256> s_cfg_search_keys {};
			static int s_cfg_sort = 0; // 0 = A-Z, 1 = newest

			auto apply_sort = [ & ]( ) {
				if ( s_cfg_sort == 0 )
					std::sort( s_config_profiles.begin( ), s_config_profiles.end( ),
						[ & ]( const cfg_entry_t& a, const cfg_entry_t& b ) { return a.name < b.name; } );
				else
					std::sort( s_config_profiles.begin( ), s_config_profiles.end( ),
						[ & ]( const cfg_entry_t& a, const cfg_entry_t& b ) { return a.time > b.time; } );
				};

			auto scan_profiles = [ & ]( ) {
				s_config_profiles.clear( );
				CreateDirectoryW( L"C:\\crisis", nullptr );
				CreateDirectoryW( L"C:\\crisis\\profiles", nullptr );
				WIN32_FIND_DATAW findData;
				HANDLE hFind = FindFirstFileW( L"C:\\crisis\\profiles\\*.bin", &findData );
				if ( hFind != INVALID_HANDLE_VALUE ) {
					do {
						std::wstring fileName = findData.cFileName;
						if ( fileName.size( ) > 4 ) {
							cfg_entry_t e;
							e.name = fileName.substr( 0, fileName.size( ) - 4 );
							ULARGE_INTEGER ul {};
							ul.LowPart = findData.ftLastWriteTime.dwLowDateTime;
							ul.HighPart = findData.ftLastWriteTime.dwHighDateTime;
							e.time = ul.QuadPart;
							SYSTEMTIME st {};
							if ( FileTimeToSystemTime( &findData.ftLastWriteTime, &st ) ) {
								wchar_t dbuf[ 32 ];
								std::swprintf( dbuf, 32, L"%02d/%02d %02d:%02d",
									( int ) st.wMonth, ( int ) st.wDay, ( int ) st.wHour, ( int ) st.wMinute );
								e.date = dbuf;
							}

							std::wstring tags_path = L"C:\\crisis\\profiles\\" + e.name + L".tags";
							std::wifstream wtag_file( tags_path );
							if ( wtag_file ) {
								std::wstring t;
								if ( std::getline( wtag_file, t ) ) {
									e.tags = t;
								}
							}

							s_config_profiles.push_back( e );
						}
					} while ( FindNextFileW( hFind, &findData ) );
					FindClose( hFind );
				}
				apply_sort( );
				if ( s_selected_profile >= ( int ) s_config_profiles.size( ) ) s_selected_profile = -1;
				s_profiles_scanned = true;
				};

			if ( !s_profiles_scanned ) scan_profiles( );

			auto lower_str = [ ]( std::wstring s ) {
				for ( auto& c : s ) if ( c >= L'A' && c <= L'Z' ) c += 32;
				return s;
				};

			// Card Background (Always drawn)
			bypass::overlay::filled_rounded_rectangle( { cfg_x, cfg_y, cfg_w, cfg_h }, 12.0f, bypass::overlay::color_t { 20, 18, 30, 235 } );
			bypass::overlay::rounded_rectangle( { cfg_x, cfg_y, cfg_w, cfg_h }, 12.0f, col_border, 1.0f );

			// Key capture logic (runs regardless of view mode)
			if ( s_is_creating ) {
				for ( int vk = 0; vk < 256; ++vk ) {
					bool down = ( GetAsyncKeyState( vk ) & 0x8000 ) != 0;
					bool pressed = down && !s_create_keys[ vk ];
					s_create_keys[ vk ] = down;
					if ( !pressed ) continue;

					std::wstring* target_str = s_create_name_focused ? &s_create_name : &s_create_tags;
					int max_len = 24;

					if ( vk == VK_ESCAPE ) {
						s_is_creating = false;
						s_create_name.clear( ); s_create_tags.clear( );
					}
					else if ( vk == VK_RETURN ) {
						if ( !s_create_name.empty( ) ) {
							std::wstring path = L"C:\\crisis\\profiles\\" + s_create_name + L".bin";
							config_snapshot_t snap = capture_config( );
							std::ofstream file( path, std::ios::binary | std::ios::trunc );
							if ( file ) file.write( reinterpret_cast< const char* >( &snap ), sizeof( snap ) );
							const bool saved = file.good( );

							if ( saved && !s_create_tags.empty( ) ) {
								std::wstring tags_path = L"C:\\crisis\\profiles\\" + s_create_name + L".tags";
								std::wofstream tag_file( tags_path );
								if ( tag_file ) tag_file << s_create_tags;
							}
							if ( saved ) {
								notifications::push( L"Config saved", s_create_name + L" was saved.", L"\uf0c7",
									bypass::overlay::color_t { 61, 200, 122, 255 } );
								s_is_creating = false; s_create_name.clear( ); s_create_tags.clear( ); s_profiles_scanned = false;
							} else {
								notifications::push( L"Save failed", L"The config could not be written.", L"\uf071",
									bypass::overlay::color_t { 220, 88, 88, 255 } );
							}
						}
					}
					else if ( vk == VK_TAB ) {
						s_create_name_focused = !s_create_name_focused;
						s_create_tags_focused = !s_create_tags_focused;
					}
					else if ( vk == VK_BACK ) { if ( !target_str->empty( ) ) target_str->pop_back( ); }
					else if ( vk == VK_SPACE && target_str->size( ) < max_len ) target_str->push_back( L' ' );
					else if ( vk >= 'A' && vk <= 'Z' && target_str->size( ) < max_len ) {
						bool shift = ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0;
						target_str->push_back( shift ? ( wchar_t ) vk : ( wchar_t ) ( vk - 'A' + L'a' ) );
					}
					else if ( vk >= '0' && vk <= '9' && target_str->size( ) < max_len ) target_str->push_back( ( wchar_t ) vk );
				}
			}
			else if ( s_is_editing ) {
				std::wstring* target_name = &s_edit_name;
				std::wstring* target_tags = &s_edit_tags;
				bool* name_foc = &s_edit_name_focused;
				bool* tags_foc = &s_edit_tags_focused;
				auto& keys = s_edit_keys;

				for ( int vk = 0; vk < 256; ++vk ) {
					bool down = ( GetAsyncKeyState( vk ) & 0x8000 ) != 0;
					bool pressed = down && !keys[ vk ];
					keys[ vk ] = down;
					if ( !pressed ) continue;

					std::wstring* target_str = *name_foc ? target_name : target_tags;
					int max_len = 24;

					if ( vk == VK_ESCAPE ) {
						s_is_editing = false;
						s_edit_name.clear( ); s_edit_tags.clear( );
					}
					else if ( vk == VK_RETURN ) {
						if ( !target_name->empty( ) ) {
							if ( s_editing_index >= 0 && s_editing_index < ( int ) s_config_profiles.size( ) ) {
								const std::wstring old_name = s_config_profiles[ s_editing_index ].name;
								std::wstring old_path = L"C:\\crisis\\profiles\\" + s_config_profiles[ s_editing_index ].name + L".bin";
								std::wstring new_path = L"C:\\crisis\\profiles\\" + *target_name + L".bin";
								bool renamed = true;
								if ( old_path != new_path ) {
									renamed = MoveFileW( old_path.c_str( ), new_path.c_str( ) ) != FALSE;
									std::wstring old_tags = L"C:\\crisis\\profiles\\" + s_config_profiles[ s_editing_index ].name + L".tags";
									std::wstring new_tags = L"C:\\crisis\\profiles\\" + *target_name + L".tags";
									if ( renamed && GetFileAttributesW( old_tags.c_str( ) ) != INVALID_FILE_ATTRIBUTES )
										MoveFileW( old_tags.c_str( ), new_tags.c_str( ) );
								}

								if ( renamed ) {
									std::wstring tags_path = L"C:\\crisis\\profiles\\" + *target_name + L".tags";
									std::wofstream tag_file( tags_path );
									if ( tag_file ) tag_file << *target_tags;
									notifications::push( L"Config edited", old_name + L" was updated.", L"\uf044",
										bypass::overlay::color_t { 61, 200, 122, 255 } );
									s_is_editing = false; s_edit_name.clear( ); s_edit_tags.clear( ); s_profiles_scanned = false;
								} else {
									notifications::push( L"Edit failed", L"The config could not be renamed.", L"\uf071",
										bypass::overlay::color_t { 220, 88, 88, 255 } );
								}
							}
						}
					}
					else if ( vk == VK_TAB ) {
						*name_foc = !*name_foc;
						*tags_foc = !*tags_foc;
					}
					else if ( vk == VK_BACK ) { if ( !target_str->empty( ) ) target_str->pop_back( ); }
					else if ( vk == VK_SPACE && target_str->size( ) < max_len ) target_str->push_back( L' ' );
					else if ( vk >= 'A' && vk <= 'Z' && target_str->size( ) < max_len ) {
						bool shift = ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0;
						target_str->push_back( shift ? ( wchar_t ) vk : ( wchar_t ) ( vk - 'A' + L'a' ) );
					}
					else if ( vk >= '0' && vk <= '9' && target_str->size( ) < max_len ) target_str->push_back( ( wchar_t ) vk );
				}
			}
			else {
				// Search key capture
				for ( int vk = 0; vk < 256; ++vk ) {
					bool down = ( GetAsyncKeyState( vk ) & 0x8000 ) != 0;
					bool pressed = down && !s_cfg_search_keys[ vk ];
					s_cfg_search_keys[ vk ] = down;
					if ( !pressed || !s_cfg_search_focused ) continue;
					if ( vk == VK_ESCAPE ) s_cfg_search_focused = false;
					else if ( vk == VK_BACK ) { if ( !s_cfg_search.empty( ) ) s_cfg_search.pop_back( ); }
					else if ( vk == VK_SPACE && s_cfg_search.size( ) < 24 ) s_cfg_search.push_back( L' ' );
					else if ( vk >= 'A' && vk <= 'Z' && s_cfg_search.size( ) < 24 ) {
						bool shift = ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0;
						s_cfg_search.push_back( shift ? ( wchar_t ) vk : ( wchar_t ) ( vk - 'A' + L'a' ) );
					}
					else if ( vk >= '0' && vk <= '9' && s_cfg_search.size( ) < 24 ) s_cfg_search.push_back( ( wchar_t ) vk );
				}
			}

			// ==========================================
			// NORMAL LIST MODE
			// ==========================================
			// ── Header: title + search + sort pills (reference toolbar style) ──
			// All outermost widgets sit at kCfgPad from either card
			// edge — one canonical padding value keeps the title,
			// search, pills, list rows, divider, action buttons and
			// row icons visually aligned instead of each element
			// picking its own margin.
			constexpr float kCfgPad = 12.0f;
			const float hdr_y = cfg_y + kCfgPad;
			constexpr float pill_w = 46.0f, pill_h = 24.0f;
			constexpr float search_h = 28.0f;
			const float pill2_x = cfg_x + cfg_w - kCfgPad - pill_w;
			const float pill1_x = pill2_x - pill_w - 8.0f;
			const float search_x = cfg_x + 98.0f;
			const float search_w = pill1_x - 12.0f - search_x;
			const bool search_hov = hit_test( mx, my, search_x, hdr_y, search_w, search_h );

			if ( clicked ) {
				if ( search_hov ) s_cfg_search_focused = true;
				else s_cfg_search_focused = false;
			}

			// Sort pills
			if ( clicked && hit_test( mx, my, pill1_x, hdr_y, pill_w, pill_h ) && s_cfg_sort != 0 ) { s_cfg_sort = 0; apply_sort( ); }
			if ( clicked && hit_test( mx, my, pill2_x, hdr_y, pill_w, pill_h ) && s_cfg_sort != 1 ) { s_cfg_sort = 1; apply_sort( ); }

			// Title
			bypass::overlay::text_ex( L"Configs", { cfg_x + kCfgPad, hdr_y, 66.0f, pill_h },
				bypass::overlay::color_t { 255, 255, 255, 245 }, 14.0f,
				bypass::overlay::text_weight_t::black, L"Geist-Bold", bypass::overlay::text_align_t::left, false );

			// Search bar
			bypass::overlay::filled_rounded_rectangle( { search_x, hdr_y, search_w, search_h }, search_h * 0.5f,
				bypass::overlay::color_t { 15, 13, 26, 255 } );
			bypass::overlay::rounded_rectangle( { search_x, hdr_y, search_w, search_h }, search_h * 0.5f,
				s_cfg_search_focused ? col_accent : bypass::overlay::color_t { 255, 255, 255, 26 }, 1.0f );
			bypass::overlay::text_ex( L"\uf002", { search_x + 10.0f, hdr_y, 14.0f, search_h },
				bypass::overlay::color_t { 160, 155, 180, 160 }, 12.0f, bypass::overlay::text_weight_t::regular,
				L"Font Awesome", bypass::overlay::text_align_t::center, false );
			bypass::overlay::text_ex( s_cfg_search.empty( ) ? L"Search configs..." : s_cfg_search.c_str( ),
				{ search_x + 30.0f, hdr_y, search_w - 40.0f, search_h },
				s_cfg_search.empty( ) ? bypass::overlay::color_t { 140, 135, 160, 140 } : bypass::overlay::color_t { 255, 255, 255, 235 },
				12.0f, bypass::overlay::text_weight_t::semi_bold, L"Inter-Medium", bypass::overlay::text_align_t::left, false );

			// Sort pills
			const wchar_t* sort_labels[ 2 ] = { L"A-Z", L"New" };
			for ( int i = 0; i < 2; ++i ) {
				const float px_ = i == 0 ? pill1_x : pill2_x;
				const bool act = s_cfg_sort == i;
				const bool hov = hit_test( mx, my, px_, hdr_y, pill_w, pill_h );
				if ( act ) {
					bypass::overlay::filled_rounded_rectangle( { px_, hdr_y, pill_w, pill_h }, pill_h * 0.5f, col_accent );
				}
				else {
					bypass::overlay::rounded_rectangle( { px_, hdr_y, pill_w, pill_h }, pill_h * 0.5f,
						hov ? bypass::overlay::color_t { 255, 255, 255, 70 } : bypass::overlay::color_t { 255, 255, 255, 70 }, 1.0f );
				}
				bypass::overlay::text_ex( sort_labels[ i ], { px_, hdr_y, pill_w, pill_h },
					act ? bypass::overlay::color_t { 255, 255, 255, 250 }
					: hov ? bypass::overlay::color_t { 210, 205, 225, 220 } : bypass::overlay::color_t { 210, 205, 225,  150 },
					10.0f, bypass::overlay::text_weight_t::bold, L"Inter-Bold", bypass::overlay::text_align_t::center, false );
			}

			// ── Column headers ──
			const float list_y = hdr_y + search_h + 16.0f;
			const float box_sz = 14.0f;
			// Colour chip aligns with the title / divider / bottom
			// buttons at kCfgPad; text columns start a few px past it.
			const float box_x = cfg_x + kCfgPad;

			// Row icon geometry (pen + trash). Both icons sit at the
			// right edge respecting kCfgPad AND are shifted 2px INNER
			// (toward the row interior) per request. Trash is right-
			// most; pen sits to its left with a fixed gap.
			constexpr float kCfgIconW      = 18.0f;
			constexpr float kCfgIconGap    = 6.0f;
			constexpr float kCfgIconInner  = 2.0f;                           // request: inner-left 2px
			const float trash_x = cfg_x + cfg_w - kCfgPad - kCfgIconW - kCfgIconInner;
			const float pen_x_base = trash_x - kCfgIconGap - kCfgIconW;

			// Define dynamic layout boundaries for text columns.
			// text_end backs off far enough to clear both icons plus
			// their inner shift; text_start leaves a small gap after
			// the colour chip.
			const float text_start = box_x + box_sz + 10.0f;
			const float text_end   = pen_x_base - 8.0f;
			const float text_width = text_end - text_start;

			// Name takes left side (35% of space)
			const float name_x = text_start;
			const float name_w = text_width * 0.35f;

			// Tags takes middle (30% of space, centered)
			const float tags_w = text_width * 0.30f;
			const float tags_x = text_start + ( text_width - tags_w ) * 0.5f;

			// Modified takes right side (35% of space, right-aligned)
			const float mod_w = text_width * 0.35f;
			const float mod_x = text_end - mod_w;

			bypass::overlay::text_ex( L"Name", { name_x, list_y, name_w, 14.0f },
				bypass::overlay::color_t { 150, 145, 170, 255 }, 10.f, bypass::overlay::text_weight_t::bold,
				L"Inter-Bold", bypass::overlay::text_align_t::left, false );

			bypass::overlay::text_ex( L"Tags", { tags_x, list_y, tags_w, 14.0f },
				bypass::overlay::color_t { 150, 145, 170, 255 }, 10.f, bypass::overlay::text_weight_t::bold,
				L"Inter-Bold", bypass::overlay::text_align_t::center, false );

			bypass::overlay::text_ex( L"Modified", { mod_x - 6.f, list_y, mod_w, 14.0f },
				bypass::overlay::color_t { 150, 145, 170, 255 }, 10.f, bypass::overlay::text_weight_t::bold,
				L"Inter-Bold", bypass::overlay::text_align_t::right, false );

			bypass::overlay::filled_rectangle( { cfg_x + kCfgPad, list_y + 18.0f, cfg_w - kCfgPad * 2.0f, 1.0f },
				bypass::overlay::color_t { 255, 255, 255, 20 } );

			const float entries_y = list_y + 26.0f;
			const float act_y = cfg_y + cfg_h - 44.0f;
			const float item_h = 36.0f;

			int max_show = ( int ) ( ( act_y - 6.0f - entries_y ) / item_h );
			if ( max_show > 8 ) max_show = 8;
			if ( s_is_creating && max_show > 0 ) max_show--;

			const std::wstring q = lower_str( s_cfg_search );

			float start_y = entries_y;
			int shown = 0;

			// ==========================================
			// INLINE CREATION ROW
			// ==========================================
			if ( s_is_creating ) {
				const float iy = entries_y;
				const float draw_y = iy;

				bool hov = hit_test( mx, my, cfg_x + kCfgPad, draw_y, cfg_w - kCfgPad * 2.0f, item_h - 2.0f );
				if ( hov ) {
					bypass::overlay::filled_rounded_rectangle( { cfg_x + kCfgPad, draw_y, cfg_w - kCfgPad * 2.0f, item_h - 2.0f }, 6.0f,
						bypass::overlay::color_t { 255, 255, 255, 10 } );
				}

				// Generic color box for new entry
				bypass::overlay::filled_rounded_rectangle( { box_x, draw_y + ( item_h - box_sz ) * 0.5f, box_sz, box_sz }, 3.0f, bypass::overlay::color_t { 120, 115, 145, 150 } );

				const float text_y = draw_y + ( item_h - 14.0f ) * 0.5f;
				bool show_cursor = ( GetTickCount( ) / 500 ) % 2 == 0;

				// Name
				std::wstring name_display = s_create_name;
				if ( s_create_name.empty( ) ) {
					name_display = L"Config name";
					bypass::overlay::text_ex( name_display.c_str( ), { name_x, text_y, name_w, 14.0f }, bypass::overlay::color_t { 120, 115, 145, 150 }, 11.0f, bypass::overlay::text_weight_t::bold, L"Inter-Medium", bypass::overlay::text_align_t::left, false );
				}
				else {
					if ( s_create_name_focused && show_cursor ) name_display += L"|";
					bypass::overlay::text_ex( name_display.c_str( ), { name_x, text_y, name_w, 14.0f }, bypass::overlay::color_t { 255, 255, 255, 255 }, 11.0f, bypass::overlay::text_weight_t::bold, L"Inter-Medium", bypass::overlay::text_align_t::left, false );
				}

				// Tags
				std::wstring tags_display = s_create_tags;
				if ( s_create_tags.empty( ) ) {
					tags_display = L"Optional tag";
					bypass::overlay::text_ex( tags_display.c_str( ), { tags_x, text_y, tags_w, 14.0f }, bypass::overlay::color_t { 120, 115, 145, 150 }, 11.0f, bypass::overlay::text_weight_t::bold, L"Inter-Medium", bypass::overlay::text_align_t::center, false );
				}
				else {
					if ( s_create_tags_focused && show_cursor ) tags_display += L"|";
					bypass::overlay::text_ex( tags_display.c_str( ), { tags_x, text_y, tags_w, 14.0f }, bypass::overlay::color_t { 150, 145, 170, 255 }, 11.0f, bypass::overlay::text_weight_t::bold, L"Inter-Medium", bypass::overlay::text_align_t::center, false );
				}

				// Modified
				bypass::overlay::text_ex( L"Now", { mod_x, text_y, mod_w, 14.0f }, bypass::overlay::color_t { 150, 145, 170, 200 }, 10.0f, bypass::overlay::text_weight_t::bold, L"Inter-Medium", bypass::overlay::text_align_t::right, false );

				if ( clicked ) {
					bool name_hov = hit_test( mx, my, name_x, draw_y, name_w, item_h );
					bool tags_hov = hit_test( mx, my, tags_x, draw_y, tags_w, item_h );
					if ( name_hov ) { s_create_name_focused = true; s_create_tags_focused = false; }
					else if ( tags_hov ) { s_create_name_focused = false; s_create_tags_focused = true; }
					else { s_create_name_focused = false; s_create_tags_focused = false; }
				}

				start_y += item_h;
				shown++;
			}

			for ( int i = 0; i < ( int ) s_config_profiles.size( ) && shown < max_show; ++i ) {
				if ( !q.empty( ) && lower_str( s_config_profiles[ i ].name ).find( q ) == std::wstring::npos )
					continue;
				const float iy = start_y + ( shown - ( s_is_creating ? 1 : 0 ) ) * item_h;
				const float draw_y = iy;
				const bool sel = ( i == s_selected_profile );
				const bool hov = hit_test( mx, my, cfg_x + kCfgPad, iy, cfg_w - kCfgPad * 2.0f, item_h - 2.0f );

				// ==========================================
				// INLINE EDITING ROW
				// ==========================================
				if ( s_is_editing && i == s_editing_index ) {
					bool show_cursor = ( GetTickCount( ) / 500 ) % 2 == 0;

					// Subtle selected background
					bypass::overlay::filled_rounded_rectangle( { cfg_x + kCfgPad, draw_y, cfg_w - kCfgPad * 2.0f, item_h - 2.0f }, 6.0f,
						bypass::overlay::color_t { 255, 255, 255, 25 } );

					std::hash<std::wstring> hasher;
					size_t h = hasher( s_edit_name );
					bypass::overlay::color_t cols[ ] = {
						{ 236, 72, 153, 255 }, { 168, 85, 247, 255 }, { 239, 68, 68, 255 },
						{ 244, 114, 182, 255 }, { 192, 132, 252, 255 }
					};
					bypass::overlay::filled_rounded_rectangle( { box_x, draw_y + ( item_h - box_sz ) * 0.5f, box_sz, box_sz }, 3.0f, cols[ h % 5 ] );

					const float text_y = draw_y + ( item_h - 14.0f ) * 0.5f;

					// Name
					std::wstring name_display = s_edit_name;
					if ( s_edit_name.empty( ) ) {
						name_display = L"Config name";
						bypass::overlay::text_ex( name_display.c_str( ), { name_x, text_y, name_w, 14.0f }, bypass::overlay::color_t { 120, 115, 145, 150 }, 11.0f, bypass::overlay::text_weight_t::bold, L"Inter-Medium", bypass::overlay::text_align_t::left, false );
					}
					else {
						if ( s_edit_name_focused && show_cursor ) name_display += L"|";
						bypass::overlay::text_ex( name_display.c_str( ), { name_x, text_y, name_w, 14.0f }, bypass::overlay::color_t { 255, 255, 255, 255 }, 11.0f, bypass::overlay::text_weight_t::bold, L"Inter-Medium", bypass::overlay::text_align_t::left, false );
					}

					// Tags
					std::wstring tags_display = s_edit_tags;
					if ( s_edit_tags.empty( ) ) {
						tags_display = L"Optional tag";
						bypass::overlay::text_ex( tags_display.c_str( ), { tags_x, text_y, tags_w, 14.0f }, bypass::overlay::color_t { 120, 115, 145, 150 }, 11.0f, bypass::overlay::text_weight_t::bold, L"Inter-Medium", bypass::overlay::text_align_t::center, false );
					}
					else {
						if ( s_edit_tags_focused && show_cursor ) tags_display += L"|";
						bypass::overlay::text_ex( tags_display.c_str( ), { tags_x, text_y, tags_w, 14.0f }, bypass::overlay::color_t { 150, 145, 170, 255 }, 11.0f, bypass::overlay::text_weight_t::bold, L"Inter-Medium", bypass::overlay::text_align_t::center, false );
					}

					// Modified
					bypass::overlay::text_ex( s_config_profiles[ i ].date.c_str( ), { mod_x, text_y, mod_w, 14.0f }, bypass::overlay::color_t { 150, 145, 170, 200 }, 10.0f, bypass::overlay::text_weight_t::bold, L"Inter-Medium", bypass::overlay::text_align_t::right, false );

					// Save (Check) / Cancel (Cross) Icons.
					// Positions derive from pen_x_base / trash_x above
					// so they inherit the same kCfgPad + inner-left
					// offset the normal-row icons use.
					const float check_x = pen_x_base;
					const bool check_hov = hit_test( mx, my, check_x, iy, kCfgIconW, item_h - 2.0f );
					const bool cross_hov = hit_test( mx, my, trash_x,  iy, kCfgIconW, item_h - 2.0f );

					bypass::overlay::text_ex( L"\uf00c", { check_x, draw_y, kCfgIconW, item_h - 2.0f },
						check_hov ? bypass::overlay::color_t { 61, 200, 122, 255 } : bypass::overlay::color_t { 61, 200, 122, 160 }, 12.0f,
						bypass::overlay::text_weight_t::bold, L"Font Awesome", bypass::overlay::text_align_t::center, false );

					bypass::overlay::text_ex( L"\uf00d", { trash_x, draw_y, kCfgIconW, item_h - 2.0f },
						cross_hov ? bypass::overlay::color_t { 239, 68, 68, 255 } : bypass::overlay::color_t { 239, 68, 68, 160 }, 12.0f,
						bypass::overlay::text_weight_t::bold, L"Font Awesome", bypass::overlay::text_align_t::center, false );

					if ( clicked ) {
						bool name_hov = hit_test( mx, my, name_x, draw_y, name_w, item_h );
						bool tags_hov = hit_test( mx, my, tags_x, draw_y, tags_w, item_h );
						if ( name_hov ) { s_edit_name_focused = true; s_edit_tags_focused = false; }
						else if ( tags_hov ) { s_edit_name_focused = false; s_edit_tags_focused = true; }
						else { s_edit_name_focused = false; s_edit_tags_focused = false; }

						if ( check_hov && !s_edit_name.empty( ) ) {
							const std::wstring old_name = s_config_profiles[ s_editing_index ].name;
							std::wstring old_path = L"C:\\crisis\\profiles\\" + s_config_profiles[ s_editing_index ].name + L".bin";
							std::wstring new_path = L"C:\\crisis\\profiles\\" + s_edit_name + L".bin";
							bool renamed = true;
							if ( old_path != new_path ) {
								renamed = MoveFileW( old_path.c_str( ), new_path.c_str( ) ) != FALSE;
								std::wstring old_tags = L"C:\\crisis\\profiles\\" + s_config_profiles[ s_editing_index ].name + L".tags";
								std::wstring new_tags = L"C:\\crisis\\profiles\\" + s_edit_name + L".tags";
								if ( renamed && GetFileAttributesW( old_tags.c_str( ) ) != INVALID_FILE_ATTRIBUTES )
									MoveFileW( old_tags.c_str( ), new_tags.c_str( ) );
							}
							if ( renamed ) {
								std::wstring tags_path = L"C:\\crisis\\profiles\\" + s_edit_name + L".tags";
								std::wofstream tag_file( tags_path );
								if ( tag_file ) tag_file << s_edit_tags;
								notifications::push( L"Config edited", old_name + L" was updated.", L"\uf044",
									bypass::overlay::color_t { 61, 200, 122, 255 } );
								s_is_editing = false; s_edit_name.clear( ); s_edit_tags.clear( ); s_profiles_scanned = false;
							} else {
								notifications::push( L"Edit failed", L"The config could not be renamed.", L"\uf071",
									bypass::overlay::color_t { 220, 88, 88, 255 } );
							}
						}
						else if ( cross_hov ) {
							s_is_editing = false; s_edit_name.clear( ); s_edit_tags.clear( );
						}
					}

					++shown;
					continue;
				}

				// Icon geometry mirrors the edit-row block above so both
				// modes render + hit-test at the exact same pixel column.
				const float pen_x = pen_x_base;
				const bool pen_hov   = hit_test( mx, my, pen_x,  iy, kCfgIconW, item_h - 2.0f );
				const bool trash_hov = hit_test( mx, my, trash_x, iy, kCfgIconW, item_h - 2.0f );

				if ( hov || sel ) {
					bypass::overlay::filled_rounded_rectangle( { cfg_x + 10.0f, draw_y, cfg_w - 20.0f, item_h - 2.0f }, 6.0f,
						sel ? bypass::overlay::color_t { 255, 255, 255, 25 }
					: bypass::overlay::color_t { 255, 255, 255, 10 } );
				}

				// Random color box
				std::hash<std::wstring> hasher;
				size_t h = hasher( s_config_profiles[ i ].name );
				bypass::overlay::color_t cols[ ] = {
					{ 236, 72, 153, 255 }, { 168, 85, 247, 255 }, { 239, 68, 68, 255 },
					{ 244, 114, 182, 255 }, { 192, 132, 252, 255 }
				};
				auto box_col = sel ? cols[ h % 5 ] : bypass::overlay::color_t { 120, 115, 145, 150 };
				bypass::overlay::filled_rounded_rectangle( { box_x, draw_y + ( item_h - box_sz ) * 0.5f, box_sz, box_sz }, 3.0f, box_col );

				// Vertically centered text Y
				const float text_y = draw_y + ( item_h - 14.0f ) * 0.5f;

				// Name (Left)
				bypass::overlay::text_ex( s_config_profiles[ i ].name.c_str( ),
					{ name_x, text_y, name_w, 14.0f },
					sel ? bypass::overlay::color_t { 255, 255, 255, 255 } : bypass::overlay::color_t { 220, 220, 220, 200 }, 11.0f,
					bypass::overlay::text_weight_t::bold, L"Inter-Medium", bypass::overlay::text_align_t::left, false );

				// Tags (Middle)
				const std::wstring& tags = s_config_profiles[ i ].tags;
				bypass::overlay::text_ex( tags.empty( ) ? L"-" : tags.c_str( ),
					{ tags_x, text_y, tags_w, 14.0f },
					tags.empty( ) ? bypass::overlay::color_t { 120, 115, 145, 150 } : bypass::overlay::color_t { 150, 145, 170, 200 }, 11.0f,
					bypass::overlay::text_weight_t::bold, L"Inter-Medium", bypass::overlay::text_align_t::center, false );

				// Modified (Right)
				bypass::overlay::text_ex( s_config_profiles[ i ].date.c_str( ),
					{ mod_x, text_y, mod_w, 14.0f },
					bypass::overlay::color_t { 150, 145, 170, 200 }, 10.0f,
					bypass::overlay::text_weight_t::bold, L"Inter-Medium", bypass::overlay::text_align_t::right, false );

				// Per-entry edit icon (Pen).
				bypass::overlay::text_ex( L"\uf304", { pen_x, draw_y, kCfgIconW, item_h - 2.0f },
					pen_hov ? bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 255 }
					: hov ? bypass::overlay::color_t { 150, 145, 170, 160 } : bypass::overlay::color_t { 150, 145, 170, 90 }, 11.0f,
					bypass::overlay::text_weight_t::bold, L"Font Awesome", bypass::overlay::text_align_t::center, false );

				// Per-entry delete icon.
				bypass::overlay::text_ex( L"\uf2ed", { trash_x, draw_y, kCfgIconW, item_h - 2.0f },
					trash_hov ? bypass::overlay::color_t { 239, 68, 68, 255 }
					: hov ? bypass::overlay::color_t { 150, 145, 170, 160 } : bypass::overlay::color_t { 150, 145, 170, 90 }, 11.0f,
					bypass::overlay::text_weight_t::bold, L"Font Awesome", bypass::overlay::text_align_t::center, false );

				if ( clicked && !s_is_editing ) {
					if ( trash_hov ) {
						const std::wstring deleted_name = s_config_profiles[ i ].name;
						std::wstring path = L"C:\\crisis\\profiles\\" + s_config_profiles[ i ].name + L".bin";
						const bool deleted = DeleteFileW( path.c_str( ) ) != FALSE;
						std::wstring tags_path = L"C:\\crisis\\profiles\\" + s_config_profiles[ i ].name + L".tags";
						if ( deleted ) {
							DeleteFileW( tags_path.c_str( ) );
							notifications::push( L"Config deleted", deleted_name + L" was deleted.", L"\uf2ed",
								bypass::overlay::color_t { 220, 88, 88, 255 } );
							s_profiles_scanned = false;
							s_selected_profile = -1;
						} else {
							notifications::push( L"Delete failed", L"The config could not be deleted.", L"\uf071",
								bypass::overlay::color_t { 220, 88, 88, 255 } );
						}
					}
					else if ( pen_hov ) {
						s_is_editing = true;
						s_editing_index = i;
						s_edit_name = s_config_profiles[ i ].name;
						s_edit_tags = s_config_profiles[ i ].tags;
						s_edit_keys.fill( false );
						s_edit_name_focused = true;
						s_edit_tags_focused = false;
					}
					else if ( hov ) {
						// Unselect capability
						if ( s_selected_profile == i )
							s_selected_profile = -1;
						else
							s_selected_profile = i;
					}
				}
				++shown;
			}

			if ( shown == 0 && !s_is_creating ) {
				bypass::overlay::text_ex( s_cfg_search.empty( ) ? L"No profiles found" : L"No matches",
					{ cfg_x, entries_y + 16.0f, cfg_w, 40.0f },
					bypass::overlay::color_t { 150, 150, 150, 150 }, 11.0f,
					bypass::overlay::text_weight_t::regular, L"Inter-Regular", bypass::overlay::text_align_t::center, false );
			}

			// ==========================================
			// BOTTOM ACTION BUTTONS
			// ==========================================
			float act_h = 32.0f;
			float act_gap = 12.0f;
			// Bottom-row buttons respect the same kCfgPad the title,
			// divider, list rows and icons use — so left button sits
			// at cfg_x + kCfgPad and right button ends at cfg_x + cfg_w
			// - kCfgPad, matching the rest of the card horizontally.
			float act_w = ( cfg_w - kCfgPad * 2.0f - act_gap ) * 0.5f;
			float btn1_x = cfg_x + kCfgPad;
			float btn2_x = btn1_x + act_w + act_gap;
			bool btn1_hov = hit_test( mx, my, btn1_x, act_y, act_w, act_h );
			bool btn2_hov = hit_test( mx, my, btn2_x, act_y, act_w, act_h );
			bool has_sel = s_selected_profile >= 0 && s_selected_profile < ( int ) s_config_profiles.size( );

			if ( s_is_creating ) {
				// Cancel & Create
				bypass::overlay::filled_rounded_rectangle( { btn1_x, act_y, act_w, act_h }, 8.0f,
					btn1_hov ? bypass::overlay::color_t { 60, 60, 70, 255 } : bypass::overlay::color_t { 40, 40, 50, 255 } );
				bypass::overlay::text_ex( L"Cancel", { btn1_x, act_y, act_w, act_h },
					btn1_hov ? bypass::overlay::color_t { 255, 255, 255, 255 } : bypass::overlay::color_t { 200, 200, 200, 200 }, 12.0f, bypass::overlay::text_weight_t::bold,
					L"Inter-Bold", bypass::overlay::text_align_t::center, false );

				bool can_create = !s_create_name.empty( );
				bypass::overlay::filled_rounded_rectangle( { btn2_x, act_y, act_w, act_h }, 8.0f,
					can_create ? ( btn2_hov ? col_accent : bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 200 } )
					: bypass::overlay::color_t { 50, 50, 50, 150 } );
				bypass::overlay::text_ex( L"Create", { btn2_x, act_y, act_w, act_h },
					can_create ? bypass::overlay::color_t { 255, 255, 255, 255 } : bypass::overlay::color_t { 100, 100, 100, 150 },
					12.0f, bypass::overlay::text_weight_t::bold, L"Inter-Bold", bypass::overlay::text_align_t::center, false );

				if ( clicked && btn1_hov ) {
					s_is_creating = false;
					s_create_name.clear( ); s_create_tags.clear( );
				}
				if ( clicked && btn2_hov && can_create ) {
					std::wstring path = L"C:\\crisis\\profiles\\" + s_create_name + L".bin";
					config_snapshot_t snap = capture_config( );
					std::ofstream file( path, std::ios::binary | std::ios::trunc );
					if ( file ) file.write( reinterpret_cast< const char* >( &snap ), sizeof( snap ) );
					const bool saved = file.good( );

					if ( saved && !s_create_tags.empty( ) ) {
						std::wstring tags_path = L"C:\\crisis\\profiles\\" + s_create_name + L".tags";
						std::wofstream tag_file( tags_path );
						if ( tag_file ) tag_file << s_create_tags;
					}
					if ( saved ) {
						notifications::push( L"Config saved", s_create_name + L" was saved.", L"\uf0c7",
							bypass::overlay::color_t { 61, 200, 122, 255 } );
						s_is_creating = false; s_create_name.clear( ); s_create_tags.clear( ); s_profiles_scanned = false;
					} else {
						notifications::push( L"Save failed", L"The config could not be written.", L"\uf071",
							bypass::overlay::color_t { 220, 88, 88, 255 } );
					}
				}
			}
			else if ( !has_sel ) {
				// Import & Save
				bypass::overlay::filled_rounded_rectangle( { btn1_x, act_y, act_w, act_h }, 8.0f,
					btn1_hov ? bypass::overlay::color_t { 60, 60, 70, 255 } : bypass::overlay::color_t { 40, 40, 50, 255 } );
				bypass::overlay::text_ex( L"Import", { btn1_x, act_y, act_w, act_h },
					bypass::overlay::color_t { 220, 220, 220, 255 }, 12.0f, bypass::overlay::text_weight_t::bold,
					L"Inter-Bold", bypass::overlay::text_align_t::center, false );

				bypass::overlay::filled_rounded_rectangle( { btn2_x, act_y, act_w, act_h }, 8.0f,
					btn2_hov ? col_accent : bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 200 } );
				bypass::overlay::text_ex( L"Save", { btn2_x, act_y, act_w, act_h },
					bypass::overlay::color_t { 255, 255, 255, 255 }, 12.0f, bypass::overlay::text_weight_t::bold,
					L"Inter-Bold", bypass::overlay::text_align_t::center, false );

				if ( clicked && btn1_hov ) {
					OPENFILENAMEW ofn;
					wchar_t szFile[ 260 ] = { 0 };
					ZeroMemory( &ofn, sizeof( ofn ) );
					ofn.lStructSize = sizeof( ofn );
					ofn.hwndOwner = NULL;
					ofn.lpstrFile = szFile;
					ofn.nMaxFile = sizeof( szFile ) / sizeof( wchar_t );
					ofn.lpstrFilter = L"Crisis Config (*.bin)\0*.bin\0";
					ofn.nFilterIndex = 1;
					ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

					if ( GetOpenFileNameW( &ofn ) ) {
						std::wstring src_path = szFile;
						const auto separator = src_path.find_last_of( L"\\/" );
						const std::wstring filename = separator == std::wstring::npos
							? src_path : src_path.substr( separator + 1 );
						const bool valid_extension = filename.size( ) > 4 &&
							_wcsicmp( filename.c_str( ) + filename.size( ) - 4, L".bin" ) == 0;

						config_snapshot_t imported{};
						const bool valid_config = valid_extension && read_config_snapshot( src_path, imported );
						if ( !valid_config ) {
							notifications::push( L"Import failed", L"Select a valid Crisis config.", L"\uf071",
								bypass::overlay::color_t { 220, 88, 88, 255 } );
						} else {
							CreateDirectoryW( L"C:\\crisis", nullptr );
							CreateDirectoryW( L"C:\\crisis\\profiles", nullptr );
							const std::wstring dest_path = L"C:\\crisis\\profiles\\" + filename;
							if ( !CopyFileW( src_path.c_str( ), dest_path.c_str( ), FALSE ) ) {
								notifications::push( L"Import failed", L"The config could not be copied.", L"\uf071",
									bypass::overlay::color_t { 220, 88, 88, 255 } );
							} else {
								const std::wstring src_tags = src_path.substr( 0, src_path.size( ) - 4 ) + L".tags";
								const std::wstring dest_tags = dest_path.substr( 0, dest_path.size( ) - 4 ) + L".tags";
								if ( GetFileAttributesW( src_tags.c_str( ) ) != INVALID_FILE_ATTRIBUTES )
									CopyFileW( src_tags.c_str( ), dest_tags.c_str( ), FALSE );
								else
									DeleteFileW( dest_tags.c_str( ) );

								apply_config( imported );
								s_profiles_scanned = false;
								scan_profiles( );
								const std::wstring imported_name = filename.substr( 0, filename.size( ) - 4 );
								s_selected_profile = -1;
								for ( std::size_t i = 0; i < s_config_profiles.size( ); ++i ) {
									if ( _wcsicmp( s_config_profiles[ i ].name.c_str( ), imported_name.c_str( ) ) == 0 ) {
										s_selected_profile = static_cast< int >( i );
										break;
									}
								}
								g_active_profile = imported_name;
								notifications::push( L"Config imported", imported_name + L" was imported and loaded.", L"\uf56f",
									bypass::overlay::color_t { 61, 200, 122, 255 } );
							}
						}
					}
				}
				if ( clicked && btn2_hov ) {
					s_is_creating = true;
					s_create_name.clear( );
					s_create_tags.clear( );
					s_create_keys.fill( false );
					s_create_name_focused = true;
					s_create_tags_focused = false;
				}
			}
			else {
				// Open Folder & Load
				bypass::overlay::filled_rounded_rectangle( { btn1_x, act_y, act_w, act_h }, 8.0f,
					btn1_hov ? bypass::overlay::color_t { 60, 60, 70, 255 } : bypass::overlay::color_t { 40, 40, 50, 255 } );
				bypass::overlay::text_ex( L"Open Folder", { btn1_x, act_y, act_w, act_h },
					bypass::overlay::color_t { 220, 220, 220, 255 }, 12.0f, bypass::overlay::text_weight_t::bold,
					L"Inter-Bold", bypass::overlay::text_align_t::center, false );

				bypass::overlay::filled_rounded_rectangle( { btn2_x, act_y, act_w, act_h }, 8.0f,
					btn2_hov ? bypass::overlay::color_t { 61, 200, 122, 255 } : bypass::overlay::color_t { 61, 200, 122, 180 } );
				bypass::overlay::text_ex( L"Load", { btn2_x, act_y, act_w, act_h },
					bypass::overlay::color_t { 255, 255, 255, 255 }, 12.0f, bypass::overlay::text_weight_t::bold,
					L"Inter-Bold", bypass::overlay::text_align_t::center, false );

				if ( clicked && btn1_hov ) {
					ShellExecuteW( NULL, L"explore", L"C:\\crisis\\profiles", NULL, NULL, SW_SHOWNORMAL );
				}
				if ( clicked && btn2_hov && !s_is_editing ) {
					std::wstring path = L"C:\\crisis\\profiles\\" + s_config_profiles[ s_selected_profile ].name + L".bin";
					bool loaded = false;
					config_snapshot_t snap {};
					if ( read_config_snapshot( path, snap ) ) {
						apply_config( snap );
						loaded = true;
						g_active_profile = s_config_profiles[ s_selected_profile ].name;
					}
					if ( loaded )
						notifications::push( L"Config loaded", s_config_profiles[ s_selected_profile ].name + L" was loaded.", L"\uf019",
							bypass::overlay::color_t { 61, 200, 122, 255 } );
					else
						notifications::push( L"Load failed", L"The config is missing or invalid.", L"\uf071",
							bypass::overlay::color_t { 220, 88, 88, 255 } );
				}
			}

			// ==========================================
			// LIVE FPS GRAPH (RIGHT SIDE)
			// ==========================================
			static float s_fps_last_now = -1.0f;
			static std::array< float, 64 > s_frame_ms_samples {};
			static std::size_t s_frame_ms_count = 0;
			static std::size_t s_frame_ms_cursor = 0;
			static float s_rolling_frame_ms = 0.0f;
			static float s_frame_stability = 0.0f;
			static std::array< float, 32 > s_display_graph_fps {};
			static std::array< float, 32 > s_graph_start_fps {};
			static std::array< float, 32 > s_graph_target_fps {};


			// ── Rolling frame-time samples used by both the badge and graph ──
			const float fps_now = std::chrono::duration< float >(
				std::chrono::steady_clock::now( ).time_since_epoch( ) ).count( );
			const float fps_dt = ( s_fps_last_now < 0.0f ) ? 0.016f : ( fps_now - s_fps_last_now );
			s_fps_last_now = fps_now;
			if ( fps_dt > 0.0001f && fps_dt < 1.0f ) {
				s_frame_ms_samples[ s_frame_ms_cursor ] = fps_dt * 1000.0f;
				s_frame_ms_cursor = ( s_frame_ms_cursor + 1 ) % s_frame_ms_samples.size( );
				s_frame_ms_count = ( std::min ) ( s_frame_ms_count + 1, s_frame_ms_samples.size( ) );
				s_rolling_frame_ms = 0.0f;
				for ( std::size_t i = 0; i < s_frame_ms_count; ++i )
					s_rolling_frame_ms += s_frame_ms_samples[ i ];
				s_rolling_frame_ms /= ( float ) s_frame_ms_count;
				float variance = 0.0f;
				for ( std::size_t i = 0; i < s_frame_ms_count; ++i ) {
					const float difference = s_frame_ms_samples[ i ] - s_rolling_frame_ms;
					variance += difference * difference;
				}
				variance /= ( float ) s_frame_ms_count;
				const float variation = std::sqrt( variance ) / s_rolling_frame_ms;
				s_frame_stability = ( std::clamp ) ( ( 1.0f - variation ) * 100.0f, 0.0f, 100.0f );
			}

			// Performance Settings card
			bypass::overlay::filled_rounded_rectangle(
				{ settings_x, settings_y, settings_w, settings_h }, 12.0f,
				bypass::overlay::color_t { 22, 19, 35, 180 } );
			bypass::overlay::rounded_rectangle(
				{ settings_x, settings_y, settings_w, settings_h }, 12.0f, col_border, 1.0f );
			bypass::overlay::text_ex( L"Performance Settings",
				{ settings_x + 16.0f, settings_y + 8.0f, settings_w - 32.0f, 28.0f },
				bypass::overlay::color_t { 245, 243, 250, 235 }, 14.0f,
				bypass::overlay::text_weight_t::bold, L"Geist-Bold",
				bypass::overlay::text_align_t::center, false );
			auto draw_faded_divider = [ & ]( float card_x, float card_w, float y ) {
				constexpr int segments = 64;
				constexpr float edge_fade = 0.38f;
				constexpr float max_alpha = 24.0f;
				const float x = card_x + 14.0f;
				const float w = card_w - 28.0f;
				for ( int i = 0; i < segments; ++i ) {
					const float x0 = x + w * ( ( float ) i / segments );
					const float x1 = x + w * ( ( float ) ( i + 1 ) / segments );
					const float position = ( ( float ) i + 0.5f ) / segments;
					float fade = ( std::min ) ( position, 1.0f - position ) / edge_fade;
					fade = ( std::clamp ) ( fade, 0.0f, 1.0f );
					fade = fade * fade * ( 3.0f - 2.0f * fade );
					const auto alpha = static_cast< std::uint8_t >( max_alpha * fade );
					if ( alpha > 0 )
						bypass::overlay::filled_rectangle( { x0, y, x1 - x0, 1.0f },
							bypass::overlay::color_t { 255, 255, 255, alpha } );
				}
				};
			draw_faded_divider( settings_x, settings_w, settings_y + 39.0f );

			const float controls_x = settings_x + 16.0f;
			const float controls_w = settings_w - 32.0f;
			widget_ctx_t settings_widget_ctx {
				mx, my, lmb, clicked, s_last_animation_delta, col_accent, col_border,
				settings_y + settings_h
			};
			float vsync_y = settings_y + 52.0f;
			float performance_mode_y = settings_y + 52.0f;
			draw_checkbox( settings_widget_ctx, controls_x, vsync_y, L"Frame Limiter", &config::settings::frame_limiter );
			draw_checkbox( settings_widget_ctx, settings_x + settings_w * 0.53f,
				performance_mode_y, L"Performance Mode", &config::settings::performance_mode );

			const float cap_y = settings_y + 82.0f;
			const float cap_track_x = settings_x + 82.0f;
			const float cap_track_w = settings_w - 162.0f;
			const bool cap_hovered = hit_test( mx, my, cap_track_x - 5.0f, cap_y, cap_track_w + 10.0f, 20.0f );
			if ( lmb && cap_hovered ) {
				const float ratio = ( std::clamp ) ( ( mx - cap_track_x ) / cap_track_w, 0.0f, 1.0f );
				config::settings::framerate_cap = std::round( 30.0f + ratio * 970.0f );
			}
			const float cap_ratio = ( config::settings::framerate_cap - 30.0f ) / 970.0f;
			bypass::overlay::text_ex( L"FPS Cap", { controls_x, cap_y, 58.0f, 20.0f },
				bypass::overlay::color_t { 205, 201, 216, 190 }, 11.0f,
				bypass::overlay::text_weight_t::semi_bold, L"Inter-Medium",
				bypass::overlay::text_align_t::left, false );
			bypass::overlay::filled_rounded_rectangle(
				{ cap_track_x, cap_y + 8.0f, cap_track_w, 4.0f }, 2.0f,
				bypass::overlay::color_t { 255, 255, 255, 20 } );
			bypass::overlay::filled_rounded_rectangle(
				{ cap_track_x, cap_y + 8.0f, cap_track_w * cap_ratio, 4.0f }, 2.0f,
				bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 205 } );
			bypass::overlay::filled_circle(
				{ cap_track_x + cap_track_w * cap_ratio, cap_y + 10.0f }, 5.0f,
				bypass::overlay::color_t { 238, 232, 250, 245 } );
			wchar_t cap_text[ 16 ] {};
			std::swprintf( cap_text, std::size( cap_text ), L"%.0f FPS", config::settings::framerate_cap );
			bypass::overlay::text_ex( cap_text,
				{ settings_x + settings_w - 72.0f, cap_y, 56.0f, 20.0f },
				bypass::overlay::color_t { 235, 231, 242, 220 }, 10.5f,
				bypass::overlay::text_weight_t::bold, L"Inter-Bold",
				bypass::overlay::text_align_t::right, false );
			draw_faded_divider( settings_x, settings_w, settings_y + 111.0f );

			wchar_t stability_text[ 16 ] {};
			wchar_t frame_rate_text[ 16 ] {};
			const float rolling_fps = s_rolling_frame_ms > 0.0f ? 1000.0f / s_rolling_frame_ms : 0.0f;
			std::swprintf( stability_text, std::size( stability_text ), L"%.0f%%", s_frame_stability );
			std::swprintf( frame_rate_text, std::size( frame_rate_text ), L"%.0f FPS", rolling_fps );
			const float metric_y = settings_y + 117.0f;
			const float metric_w = settings_w * 0.5f;
			bypass::overlay::text_ex( L"STABILITY",
				{ settings_x + 16.0f, metric_y, metric_w - 24.0f, 11.0f },
				bypass::overlay::color_t { 155, 151, 168, 145 }, 7.5f,
				bypass::overlay::text_weight_t::bold, L"Inter-Bold",
				bypass::overlay::text_align_t::left, false );
			bypass::overlay::text_ex( stability_text,
				{ settings_x + 16.0f, metric_y + 11.0f, metric_w - 24.0f, 16.0f },
				bypass::overlay::color_t { 235, 232, 242, 225 }, 10.5f,
				bypass::overlay::text_weight_t::bold, L"Inter-Bold",
				bypass::overlay::text_align_t::left, false );
			bypass::overlay::text_ex( L"FRAME RATE",
				{ settings_x + metric_w, metric_y, metric_w - 16.0f, 11.0f },
				bypass::overlay::color_t { 155, 151, 168, 145 }, 7.5f,
				bypass::overlay::text_weight_t::bold, L"Inter-Bold",
				bypass::overlay::text_align_t::right, false );
			bypass::overlay::text_ex( frame_rate_text,
				{ settings_x + metric_w, metric_y + 11.0f, metric_w - 16.0f, 16.0f },
				bypass::overlay::color_t { 235, 232, 242, 225 }, 10.5f,
				bypass::overlay::text_weight_t::bold, L"Inter-Bold",
				bypass::overlay::text_align_t::right, false );

			// Menu Settings card
			bypass::overlay::filled_rounded_rectangle(
				{ menu_settings_x, menu_settings_y, menu_settings_w, menu_settings_h }, 12.0f,
				bypass::overlay::color_t { 22, 19, 35, 180 } );
			bypass::overlay::rounded_rectangle(
				{ menu_settings_x, menu_settings_y, menu_settings_w, menu_settings_h }, 12.0f,
				col_border, 1.0f );
			bypass::overlay::text_ex( L"Menu Settings",
				{ menu_settings_x + 16.0f, menu_settings_y + 8.0f, menu_settings_w - 32.0f, 28.0f },
				bypass::overlay::color_t { 245, 243, 250, 235 }, 14.0f,
				bypass::overlay::text_weight_t::bold, L"Geist-Bold",
				bypass::overlay::text_align_t::center, false );
			const float notify_test_x = menu_settings_x + menu_settings_w - 39.0f;
			const float notify_test_y = menu_settings_y + 8.0f;
			const bool notify_test_hovered = hit_test(
				mx, my, notify_test_x, notify_test_y, 25.0f, 25.0f );
			if ( clicked && notify_test_hovered )
				notifications::push( L"Configuration saved",
					L"Your performance settings were updated.", L"\uf00c",
					bypass::overlay::color_t { 61, 200, 122, 255 } );
			bypass::overlay::filled_rounded_rectangle(
				{ notify_test_x, notify_test_y, 25.0f, 25.0f }, 7.0f,
				notify_test_hovered
				? bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 34 }
			: bypass::overlay::color_t { 255, 255, 255, 8 } );
			bypass::overlay::rounded_rectangle(
				{ notify_test_x, notify_test_y, 25.0f, 25.0f }, 7.0f,
				bypass::overlay::color_t { 70, 66, 92, static_cast< std::uint8_t >( notify_test_hovered ? 150u : 95u ) }, 1.0f );
			bypass::overlay::text_ex( L"\uf0f3",
				{ notify_test_x, notify_test_y, 25.0f, 25.0f },
				notify_test_hovered ? col_accent : bypass::overlay::color_t { 205, 201, 216, 165 },
				10.0f, bypass::overlay::text_weight_t::regular, L"Font Awesome",
				bypass::overlay::text_align_t::center, false );
			draw_faded_divider( menu_settings_x, menu_settings_w, menu_settings_y + 39.0f );

			bypass::overlay::text_ex( L"Theme Color",
				{ menu_settings_x + 16.0f, menu_settings_y + 50.0f, 90.0f, 22.0f },
				bypass::overlay::color_t { 205, 201, 216, 190 }, 11.0f,
				bypass::overlay::text_weight_t::semi_bold, L"Inter-Medium",
				bypass::overlay::text_align_t::left, false );
			static constexpr bypass::overlay::color_t theme_colors[ ] = {
				{ 150, 90, 255, 255 }, { 70, 145, 255, 255 }, { 105, 205, 175, 255 },
				{ 240, 120, 130, 255 }, { 240, 165, 100, 255 },
			};
			const float swatch_start_x = menu_settings_x + menu_settings_w - 167.0f;
			for ( std::uint32_t i = 0; i < static_cast< std::uint32_t >( std::size( theme_colors ) ); ++i ) {
				const float swatch_x = swatch_start_x + i * 25.0f;
				const bool swatch_hovered = hit_test( mx, my, swatch_x - 3.0f, menu_settings_y + 49.0f, 22.0f, 22.0f );
				if ( clicked && swatch_hovered )
					s_menu_theme = i;
				if ( s_menu_theme == i )
					bypass::overlay::rounded_rectangle(
						{ swatch_x - 2.0f, menu_settings_y + 51.0f, 20.0f, 20.0f }, 10.0f,
						bypass::overlay::color_t { 240, 237, 246, 180 }, 1.5f );
				bypass::overlay::filled_circle(
					{ swatch_x + 8.0f, menu_settings_y + 61.0f }, swatch_hovered ? 7.0f : 6.0f,
					theme_colors[ i ] );
			}
			// ── Rainbow swatch ──────────────────────────────────────────
			{
				const float swatch_x = swatch_start_x + std::size( theme_colors ) * 25.0f;
				const bool swatch_hovered = hit_test( mx, my, swatch_x - 3.0f, menu_settings_y + 49.0f, 22.0f, 22.0f );
				if ( clicked && swatch_hovered )
					s_menu_theme = rainbow_theme_index;
				if ( s_menu_theme == rainbow_theme_index )
					bypass::overlay::rounded_rectangle(
						{ swatch_x - 2.0f, menu_settings_y + 51.0f, 20.0f, 20.0f }, 10.0f,
						bypass::overlay::color_t { 240, 237, 246, 180 }, 1.5f );
				const float r = swatch_hovered ? 7.0f : 6.0f;
				const float cx = swatch_x + 8.0f;
				const float cy = menu_settings_y + 61.0f;
				bypass::overlay::filled_circle( { cx, cy }, r, rainbow_accent( 0.0f ) );
				bypass::overlay::filled_circle( { cx, cy }, r * 0.55f, rainbow_accent( 120.0f ) );
			}
			widget_ctx_t menu_widget_ctx {
				mx, my, lmb, clicked, s_last_animation_delta, col_accent, col_border,
				menu_settings_y + menu_settings_h
			};
			float notifications_y = menu_settings_y + 78.0f;
			float background_y = menu_settings_y + 78.0f;
			float streamproof_y = menu_settings_y + 110.0f;
			float menu_key_y = menu_settings_y + 110.0f;
			draw_checkbox( menu_widget_ctx, menu_settings_x + 16.0f, notifications_y,
				L"Show Notifications", &s_show_notifications );
			draw_checkbox( menu_widget_ctx, menu_settings_x + menu_settings_w * 0.56f, background_y,
				L"Show Background", &s_show_background );
			draw_checkbox( menu_widget_ctx, menu_settings_x + 16.0f, streamproof_y,
				L"Streamproof", &s_streamproof );
			draw_keybind( menu_widget_ctx, menu_settings_x + menu_settings_w * 0.56f, menu_key_y,
				menu_settings_w * 0.44f - 16.0f, L"Menu Key", &s_menu_key );

			// Card Background (Matches Configurations Card)
			bypass::overlay::filled_rounded_rectangle(
				{ perf_x, perf_y, perf_w, perf_h }, 12.0f,
				bypass::overlay::color_t { 20, 18, 30, 235 } );
			bypass::overlay::rounded_rectangle(
				{ perf_x, perf_y, perf_w, perf_h }, 12.0f, col_border, 1.0f );

			// ── Header row: title, info, and live FPS badge ──
			bypass::overlay::text_ex( L"FPS Performance",
				{ perf_x + 16.0f, perf_y + 12.0f, 150.0f, 18.0f },
				bypass::overlay::color_t { 245, 245, 245, 245 }, 12.5f,
				bypass::overlay::text_weight_t::bold, L"Inter-Bold",
				bypass::overlay::text_align_t::left, false );
			constexpr float fps_badge_w = 94.0f;
			constexpr float fps_badge_h = 24.0f;
			const float fps_badge_x = perf_x + perf_w - fps_badge_w - 14.0f;
			const float fps_badge_y = perf_y + 9.0f;
			wchar_t fps_badge_text[ 16 ] {};
			std::swprintf( fps_badge_text, std::size( fps_badge_text ), L"AVG %.1f ms", s_rolling_frame_ms );
			bypass::overlay::filled_rounded_rectangle(
				{ fps_badge_x, fps_badge_y, fps_badge_w, fps_badge_h }, 7.0f,
				bypass::overlay::color_t { 28, 25, 40, 220 } );
			bypass::overlay::rounded_rectangle(
				{ fps_badge_x, fps_badge_y, fps_badge_w, fps_badge_h }, 7.0f,
				bypass::overlay::color_t { 70, 66, 92, 110 }, 1.0f );
			bypass::overlay::text_ex( L"\uf017",
				{ fps_badge_x + 7.0f, fps_badge_y, 15.0f, fps_badge_h },
				bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 210 }, 10.0f,
				bypass::overlay::text_weight_t::bold, L"Font Awesome",
				bypass::overlay::text_align_t::center, false );
			bypass::overlay::text_ex( fps_badge_text,
				{ fps_badge_x + 23.0f, fps_badge_y, fps_badge_w - 28.0f, fps_badge_h },
				bypass::overlay::color_t { 220, 216, 232, 215 }, 10.0f,
				bypass::overlay::text_weight_t::bold, L"Inter-Medium",
				bypass::overlay::text_align_t::center, false );

			// ── Plot geometry ──
			const float axis_w = 36.0f;
			const float plot_x = perf_x + axis_w;
			const float plot_y = perf_y + 46.0f;
			const float plot_w = perf_w - axis_w - 14.0f;
			const float plot_h = perf_h - 46.0f - 30.0f;
			static float s_graph_snapshot_timer = 0.0f;
			static float s_graph_animation = 1.0f;
			static bool s_graph_initialized = false;
			static float s_graph_max_fps = 0.0f; // <-- Add this line
			// A stable scale prevents old samples jumping when a new high is recorded.
			constexpr float nice_min = 0.0f;
			// Dynamically pad the max based on the highest FPS seen so far (minimum 60)
			const float nice_max = ( std::max ) ( 60.0f, s_graph_max_fps * 1.15f );
			const float fps_range = nice_max - nice_min;

			// Y labels + horizontal gridlines
			for ( int k = 0; k <= 5; ++k ) {
				const float frac = ( float ) k / 5.0f;
				const float gy = plot_y + plot_h * ( 1.0f - frac );
				wchar_t ybuf[ 16 ];
				const float val = nice_min + fps_range * frac;

				// Format as "1.5K" for values >= 1000, otherwise standard integer
				if ( val >= 1000.0f )
					std::swprintf( ybuf, 16, L"%.1fK", val / 1000.0f );
				else
					std::swprintf( ybuf, 16, L"%.0f", val );

				bypass::overlay::text_ex( ybuf, { perf_x + 8.0f, gy - 7.0f, axis_w - 14.0f, 14.0f },
					bypass::overlay::color_t { 120, 120, 125, 200 }, 9.0f,
					bypass::overlay::text_weight_t::regular, L"Inter-Regular",
					bypass::overlay::text_align_t::right, false );
				bypass::overlay::line( { plot_x, gy }, { plot_x + plot_w, gy },
					bypass::overlay::color_t { 255, 255, 255, 10 }, 1.0f );
			}

			// Vertical guides
			constexpr int x_labels = 7;
			for ( int k = 0; k < x_labels; ++k ) {
				const float frac = ( float ) k / ( float ) ( x_labels - 1 );
				const float gx = plot_x + plot_w * frac;
				bypass::overlay::line( { gx, plot_y }, { gx, plot_y + plot_h },
					bypass::overlay::color_t { 255, 255, 255, 8 }, 1.0f );
			}

			bypass::overlay::text_ex( L"64 frame rolling sample",
				{ plot_x, plot_y + plot_h + 8.0f, plot_w, 14.0f },
				bypass::overlay::color_t { 120, 120, 125, 190 }, 10.0f,
				bypass::overlay::text_weight_t::bold, L"Inter-Regular",
				bypass::overlay::text_align_t::center, false );

			// Summarize the window, then animate between complete graph snapshots.
						// Summarize the window, then animate between complete graph snapshots.
			if ( s_frame_ms_count > 1 ) {
				auto point_y = [ & ]( float fps ) {
					const float normalized = ( std::clamp ) ( ( fps - nice_min ) / fps_range, 0.0f, 1.0f );
					return plot_y + plot_h * ( 1.0f - normalized );
					};
				constexpr std::size_t frames_per_point = 2;
				const std::size_t graph_point_count = ( s_frame_ms_count + frames_per_point - 1 ) / frames_per_point;
				const std::size_t oldest = s_frame_ms_count == s_frame_ms_samples.size( )
					? s_frame_ms_cursor : 0;
				std::array< float, 32 > sampled_graph_fps {};

				for ( std::size_t point = 0; point < graph_point_count; ++point ) {
					const std::size_t first_sample = point * frames_per_point;
					const std::size_t samples_in_point = ( std::min ) (
						frames_per_point, s_frame_ms_count - first_sample );
					float average_ms = 0.0f;
					for ( std::size_t sample = 0; sample < samples_in_point; ++sample ) {
						const std::size_t sample_index = (
							oldest + first_sample + sample ) % s_frame_ms_samples.size( );
						average_ms += s_frame_ms_samples[ sample_index ];
					}
					average_ms /= ( float ) samples_in_point;
					sampled_graph_fps[ point ] = 1000.0f / average_ms;

					// Continuously grow the max FPS ceiling so the graph never squishes 
					// when FPS drops, but avoids locking to the initial 62,500 FPS dummy frame.
					if ( sampled_graph_fps[ point ] > s_graph_max_fps )
						s_graph_max_fps = sampled_graph_fps[ point ];
				}

				s_graph_snapshot_timer += fps_dt;
				if ( !s_graph_initialized ) {
					s_display_graph_fps = sampled_graph_fps;
					s_graph_start_fps = sampled_graph_fps;
					s_graph_target_fps = sampled_graph_fps;
					s_graph_initialized = true;
				}
				else if ( s_graph_snapshot_timer >= 0.45f ) {
					// You were missing this block! It's required to actually update the graph.
					s_graph_start_fps = s_display_graph_fps;
					s_graph_target_fps = sampled_graph_fps;
					s_graph_snapshot_timer = 0.0f;
					s_graph_animation = 0.0f;
				}

				s_graph_animation = ( std::min ) (
					1.0f, s_graph_animation + fps_dt / 1.1f );
				const float t = s_graph_animation;
				const float graph_ease = t * t * t * ( t * ( t * 6.0f - 15.0f ) + 10.0f );
				for ( std::size_t point = 0; point < graph_point_count; ++point ) {
					s_display_graph_fps[ point ] = s_graph_start_fps[ point ]
						+ ( s_graph_target_fps[ point ] - s_graph_start_fps[ point ] ) * graph_ease;
				}
				bypass::overlay::push_clip( { plot_x, plot_y, plot_w, plot_h } );
				bypass::overlay::point_t prev_l {};
				bool have_prev = false;
				for ( std::size_t i = 0; i < graph_point_count; ++i ) {
					const float fx = graph_point_count > 1
						? plot_x + plot_w * ( float ) i / ( float ) ( graph_point_count - 1 )
						: plot_x + plot_w;
					const float ly = point_y( s_display_graph_fps[ i ] );
					if ( have_prev ) {
						bypass::overlay::line( prev_l, { fx, ly },
							bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 255 }, 2.5f );
					}
					prev_l = { fx, ly };
					have_prev = true;
				}
				bypass::overlay::pop_clip( );
			}
			else {
				bypass::overlay::text_ex( L"Collecting FPS data...",
					{ plot_x, plot_y + plot_h * 0.5f - 8.0f, plot_w, 16.0f },
					bypass::overlay::color_t { 120, 120, 125, 200 }, 10.0f,
					bypass::overlay::text_weight_t::regular, L"Inter-Regular",
					bypass::overlay::text_align_t::center, false );
			}
		}
		auto open_settings_page = [ & ]( std::uint32_t tab, std::uint32_t subtab ) {
			s_main_tab = 0;
			s_sidebar_tab = tab;
			s_sidebar_subtab = subtab;
			s_expanded_tabs |= ( 1u << tab );
			};

		auto draw_page_link = [ & ](
			float x, float y, float w, float h,
			const wchar_t* icon, const wchar_t* title, const wchar_t* description,
			std::uint32_t tab, std::uint32_t subtab ) {
				const bool hovered = hit_test( mx, my, x, y, w, h );

				bypass::overlay::filled_rounded_rectangle(
					{ x, y, w, h }, 10.0f,
					hovered
					? bypass::overlay::color_t { 27, 24, 45, 230 }
				: bypass::overlay::color_t { 22, 19, 35, 165 } );
				bypass::overlay::rounded_rectangle(
					{ x, y, w, h }, 10.0f,
					hovered
					? bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 115 }
					: col_border,
					1.0f );
				bypass::overlay::text_ex(
					icon, { x + 14.0f, y, 34.0f, h },
					hovered ? col_accent : bypass::overlay::color_t { 205, 200, 218, 145 },
					15.0f, bypass::overlay::text_weight_t::regular,
					L"Font Awesome", bypass::overlay::text_align_t::center, false );
				bypass::overlay::text_ex(
					title, { x + 58.0f, y + 11.0f, w - 92.0f, 19.0f },
					bypass::overlay::color_t { 236, 232, 243, 225 }, 14.0f,
					bypass::overlay::text_weight_t::bold,
					L"Geist-Medium", bypass::overlay::text_align_t::left, false );
				bypass::overlay::text_ex(
					description, { x + 58.0f, y + 31.0f, w - 92.0f, 18.0f },
					bypass::overlay::color_t { 205, 200, 218, 105 }, 11.0f,
					bypass::overlay::text_weight_t::regular,
					L"Inter-Regular", bypass::overlay::text_align_t::left, false );
				bypass::overlay::text_ex(
					L"", { x + w - 30.0f, y, 18.0f, h },
					bypass::overlay::color_t { 220, 216, 230, static_cast< std::uint8_t >( hovered ? 175u : 75u ) },
					9.0f, bypass::overlay::text_weight_t::regular,
					L"Font Awesome", bypass::overlay::text_align_t::center, false );

				if ( clicked && hovered )
					open_settings_page( tab, subtab );
			};

		if ( s_main_tab == 1 ) {

		}
		else {
			static std::uint32_t s_main_tab = 0;
			static std::uint32_t s_profile_page_tab = 0;
			// ---- Tabs now live in the main window --------------------------------
			static constexpr const wchar_t* page_tabs[ ] = { L"Shop", L"Gamble", L"Crates" };

			constexpr float tab_h = 36.0f;
			constexpr float tab_gap = 10.0f;
			constexpr float tab_radius = 16.0f;
			constexpr float tab_inset = 4.0f;
			const float tabs_y = page_y - 10.0f;
			const float each_tab_w = ( page_w - tab_gap * 2.0f ) / 3.0f;

			// Outer container — slightly darker so the elevated pill reads clearly
			bypass::overlay::filled_rounded_rectangle(
				{ page_x, tabs_y, page_w, tab_h }, tab_radius,
				bypass::overlay::color_t { 20, 18, 30, 235 } );

			// Smooth-slide active indicator
			static float s_tab_anim_x = -1.0f;
			const float target_anim_x = page_x + static_cast< float >( s_profile_page_tab ) * ( each_tab_w + tab_gap );
			if ( s_tab_anim_x < 0.0f )
				s_tab_anim_x = target_anim_x;
			else
				s_tab_anim_x += ( target_anim_x - s_tab_anim_x ) * 0.18f;
			if ( std::abs( target_anim_x - s_tab_anim_x ) < 0.5f )
				s_tab_anim_x = target_anim_x;

			const float pill_x = s_tab_anim_x + tab_inset;
			const float pill_y = tabs_y + 2.0f;
			const float pill_w = each_tab_w - tab_inset * 2.0f;
			const float pill_h = tab_h - 2.0f - tab_inset;
			const float pill_r = tab_radius - tab_inset;

			bypass::overlay::filled_rounded_rectangle(
				{ pill_x, pill_y, pill_w, pill_h }, pill_r,
				bypass::overlay::color_t { 48, 46, 55, 120 } );

			for ( int i = 0; i < 3; ++i ) {
				const float tab_x = page_x + static_cast< float >( i ) * ( each_tab_w + tab_gap );
				const bool tab_hovered = hit_test( mx, my, tab_x, tabs_y, each_tab_w, tab_h );
				const bool tab_active = s_profile_page_tab == static_cast< std::uint32_t >( i );

				if ( clicked && tab_hovered )
					s_profile_page_tab = static_cast< std::uint32_t >( i );

				bypass::overlay::text_ex( page_tabs[ i ],
					{ tab_x, tabs_y, each_tab_w, tab_h },
					tab_active
					? bypass::overlay::color_t { 255, 255, 255, 255 }
					: bypass::overlay::color_t { 215, 210, 228, static_cast< std::uint8_t >( tab_hovered ? 175u : 95u ) },
					13.0f, bypass::overlay::text_weight_t::semi_bold,
					L"Inter-Medium", bypass::overlay::text_align_t::center, false );
			}


			// ---- Sub-window: sketched profile card --------------------------------
			constexpr float side_w = 270.0f;
			constexpr float side_h = panel_h;

			const float side_x = ( std::min ) (
				content_x + content_w + 6.0f,
				static_cast< float >( gui::screen_width ) - side_w - 12.0f );
			const float side_y = content_y - 2.0f;

			bypass::overlay::filled_rounded_rectangle(
				{ side_x, side_y, side_w, side_h }, panel_radius,
				bypass::overlay::color_t { 15, 13, 26, 255 } );

			bypass::overlay::filled_rounded_rectangle(
				{ side_x, side_y, side_w, side_h }, panel_radius,
				bypass::overlay::color_t { 32, 29, 42, 44 } );

			bypass::overlay::rounded_rectangle(
				{ side_x, side_y, side_w, side_h }, panel_radius,
				bypass::overlay::color_t { 62, 66, 84, 135 }, 1.0f );

			// The complete profile snapshot is produced by crisis-auth. The menu only
			// renders it; it never derives levels, XP, K/D, win rate, or placement.
			const auto server_profile = crisis::profile::client.snapshot( );
			if ( server_profile.valid ) {
				if ( !server_profile.display_name.empty( ) ) s_profile_name = server_profile.display_name;
				if ( !server_profile.username.empty( ) ) s_profile_username = server_profile.username;
				s_profile_discord_id = server_profile.discord_id;
				s_profile_role = server_profile.role;
				s_profile_expires_unix = server_profile.expires_unix;
				s_fortnite_stats = { server_profile.games_played, server_profile.wins, server_profile.kills,
					server_profile.deaths, server_profile.ranked_games, server_profile.non_ranked_games,
					server_profile.tournament_games, server_profile.ranked_kills, server_profile.non_ranked_kills,
					server_profile.tournament_kills, server_profile.accounts_used, server_profile.average_game_minutes };
			}
			const float win_rate = server_profile.valid ? server_profile.winrate / 100.0f : 0.0f;
			const float kd = server_profile.valid ? server_profile.kd : 0.0f;
			const std::uint64_t xp = server_profile.valid ? server_profile.xp_balance : 0;
			const std::uint64_t lifetime_xp = server_profile.valid ? server_profile.lifetime_xp : 0;
			const std::uint64_t level = server_profile.valid ? server_profile.level : 1;
			const float level_progress = server_profile.valid ? ( std::clamp )( server_profile.level_progress, 0.0f, 1.0f ) : 0.0f;

			// Shared economy state — accessible from both Shop and Crates tabs
			static std::uint32_t s_shop_coins = 0;
			auto apply_casino_result = [ & ]( const crisis::casino::result_t& result ) {
				if ( !result.success || !result.proof_verified ) {
					const std::wstring error_text = result.error.empty( )
						? L"The casino response could not be verified."
						: crisis::casino::detail::utf8_to_wide( result.error );
					notifications::push( L"Casino request failed", error_text, L"\uf071",
						bypass::overlay::color_t { 220, 88, 88, 255 } );
					return false;
				}
				s_shop_coins = static_cast< std::uint32_t >(
					( std::min )( result.balance, static_cast< std::uint64_t >( UINT32_MAX ) ) );
				return true;
			};
			static bool s_casino_state_attempted = false;
			if ( server_profile.valid && !s_casino_state_attempted )
				s_shop_coins = static_cast< std::uint32_t >( ( std::min )( server_profile.crisis_coins, static_cast< std::uint64_t >( UINT32_MAX ) ) );
			if ( !s_casino_state_attempted ) {
				s_casino_state_attempted = true;
				if ( crisis::casino::client.configured( ) )
					apply_casino_result( crisis::casino::client.state( ) );
				else
					notifications::push( L"Casino unavailable", L"Player authentication is not configured.", L"\uf071",
						bypass::overlay::color_t { 220, 88, 88, 255 } );
			}
			static bool          s_crate_unlocked[ 12 ] = { true };
			static int           s_crate_last_result = -1;
			static float         s_crate_result_until = 0.0f;
			static bool          s_splash_active = false;
			static float         s_splash_start = 0.0f;

			// ── Shop tab content ──────────────────────────────────────────────────
			if ( s_profile_page_tab == 0 )
			{
				// Empty balance renders as "BROKE" in the same slot
				// the numeric value would use — the card's value AND
				// detail strings both switch to the "no XP" wording
				// so the big number reads "BROKE" instead of "0.0K".
				wchar_t xp_short[ 16 ] {};
				wchar_t xp_detail[ 48 ] {};
				if ( xp == 0 ) {
					std::swprintf( xp_short, std::size( xp_short ), L"BROKE" );
					std::swprintf( xp_detail, std::size( xp_detail ), L"No spendable XP" );
				}
				else {
					std::swprintf( xp_short, std::size( xp_short ), L"%.1fK",
						static_cast< double >( xp ) / 1000.0 );
					std::swprintf( xp_detail, std::size( xp_detail ), L"Available: %.1fK XP",
						static_cast< double >( xp ) / 1000.0 );
				}

				const float body_y = tabs_y + tab_h + 12.0f;
				const float body_h = ( content_y - 2.0f + panel_h ) - 20.0f - body_y;

				auto draw_currency_card = [ & ]( float cx, float cy, float cw, float ch,
					const wchar_t* icon, const wchar_t* label, const wchar_t* value,
					const wchar_t* detail, bypass::overlay::color_t card_col ) {
						bypass::overlay::filled_rounded_rectangle(
							{ cx, cy, cw, ch }, 10.0f,
							bypass::overlay::color_t { 22, 19, 35, 160 } );

						bypass::overlay::rounded_rectangle(
							{ cx, cy, cw, ch }, 10.0f, col_border, 1.0f );

						bypass::overlay::text_ex( label,
							{ cx + 14.0f, cy + 10.0f, cw - 28.0f, 14.0f },
							bypass::overlay::color_t { 213, 208, 225, 105 }, 10.0f,
							bypass::overlay::text_weight_t::semi_bold,
							L"Inter-Medium", bypass::overlay::text_align_t::left, false );

						const float value_x = cx + 14.0f;
						const float value_y = cy + 26.0f;

						bypass::overlay::text_ex( value,
							{ value_x, value_y, cw - 28.0f, 24.0f },
							bypass::overlay::color_t { 244, 241, 250, 230 }, 16.0f,
							bypass::overlay::text_weight_t::black,
							L"Geist-Bold", bypass::overlay::text_align_t::left, false );

						const float value_w = static_cast< float >( std::wcslen( value ) ) * 10.0f;

						bypass::overlay::text_ex( icon,
							{ value_x + value_w + 8.0f, value_y, 24.0f, 24.0f },
							card_col, 14.0f, bypass::overlay::text_weight_t::bold,
							L"Font Awesome", bypass::overlay::text_align_t::center, false );

						bypass::overlay::text_ex( detail,
							{ cx + 14.0f, cy + ch - 24.0f, cw - 28.0f, 14.0f },
							bypass::overlay::color_t { 222, 218, 232, 165 }, 11.0f,
							bypass::overlay::text_weight_t::bold,
							L"Inter-Bold", bypass::overlay::text_align_t::left, false );
					};

				const float card_gap = 12.0f;
				constexpr float card_w = 280.0f;
				constexpr float card_h = 92.0f;

				// Top Left: Spendable XP
				draw_currency_card( page_x, body_y, card_w, card_h,
					L"\uf0e7", L"SPENDABLE XP", xp_short, xp_detail,
					bypass::overlay::color_t { 245, 196, 66, 255 } );

				// Top Right: Lifetime XP. Same "BROKE" fallback as
				// the spendable card so both currency cards read
				// consistently on a fresh account or unauthenticated
				// session.
				wchar_t life_short[ 16 ] {};
				wchar_t life_detail[ 48 ] {};
				if ( lifetime_xp == 0 ) {
					std::swprintf( life_short, std::size( life_short ), L"BROKE" );
					std::swprintf( life_detail, std::size( life_detail ), L"No lifetime XP earned yet" );
				}
				else {
					std::swprintf( life_short, std::size( life_short ), L"%.1fK",
						static_cast< double >( lifetime_xp ) / 1000.0 );
					std::swprintf( life_detail, std::size( life_detail ), L"Permanent scoreboard score" );
				}

				draw_currency_card( page_x + card_w + card_gap, body_y, card_w, card_h,
					L"\uf201", L"LIFETIME XP", life_short, life_detail,
					col_text_active );

				// ── XP Earning Rates card ──────────────────────────────────────────────
				const float rates_x = page_x;
				const float rates_y = body_y + card_h + 12.0f;
				const float rates_w = card_w * 2.0f + card_gap;
				constexpr float rates_h = 132.0f;

				bypass::overlay::filled_rounded_rectangle(
					{ rates_x, rates_y, rates_w, rates_h }, 10.0f,
					bypass::overlay::color_t { 22, 19, 35, 160 } );
				bypass::overlay::rounded_rectangle(
					{ rates_x, rates_y, rates_w, rates_h }, 10.0f, col_border, 1.0f );

				bypass::overlay::text_ex( L"XP EARNING RATES",
					{ rates_x + 14.0f, rates_y + 13.0f, 180.0f, 14.0f },
					bypass::overlay::color_t { 213, 208, 225, 105 }, 10.0f,
					bypass::overlay::text_weight_t::semi_bold,
					L"Inter-Medium", bypass::overlay::text_align_t::left, false );

				struct rate_row_t { const wchar_t* icon; const wchar_t* text; const wchar_t* value; };
				wchar_t victory_rate[ 24 ] {}, elimination_rate[ 24 ] {};
				std::swprintf( victory_rate, std::size( victory_rate ), L"+%u XP", server_profile.victory_xp );
				std::swprintf( elimination_rate, std::size( elimination_rate ), L"+%u XP", server_profile.elimination_xp );
				const rate_row_t rate_rows[ ] = {
					{ L"\uf091", L"Victory Royale", victory_rate },
					{ L"\uf05b", L"Elimination", elimination_rate },
				};

				constexpr float rate_row_h = 20.0f;
				constexpr float rate_row_pitch = 21.5f;

				const float col1_x = rates_x + 14.0f;

				for ( int i = 0; i < 2; ++i ) {
					const float rx = col1_x;
					const float ry = rates_y + 33.0f + i * rate_row_pitch;
					const auto& row = rate_rows[ i ];

					bypass::overlay::text_ex( row.icon,
						{ rx, ry, 16.0f, rate_row_h },
						bypass::overlay::color_t { 245, 196, 66, 190 }, 10.0f,
						bypass::overlay::text_weight_t::regular,
						L"Font Awesome", bypass::overlay::text_align_t::center, false );

					bypass::overlay::text_ex( row.text,
						{ rx + 22.0f, ry, 140.0f, rate_row_h },
						bypass::overlay::color_t { 210, 205, 220, 150 }, 10.5f,
						bypass::overlay::text_weight_t::bold,
						L"Inter-Bold", bypass::overlay::text_align_t::left, false );

					bypass::overlay::text_ex( row.value,
						{ rx + 150.0f, ry, 80.0f, rate_row_h },
						bypass::overlay::color_t { 244, 241, 250, 200 }, 10.5f,
						bypass::overlay::text_weight_t::black,
						L"Geist-Bold", bypass::overlay::text_align_t::right, false );
				}

				// ── Active Boosters (Bottom Left) ─────────────────────────────────────
				const float boost_y = rates_y + 84.0f;
				const float boost_h = 36.0f;
				const float boost_gap = 8.0f;
				const float boost_area_w = rates_w * 0.45f - 24.0f;
				const float boost_w = ( boost_area_w - boost_gap ) * 0.5f;

				struct booster_t {
					const wchar_t* icon;
					const wchar_t* label;
					const wchar_t* value;
					bypass::overlay::color_t col;
				};

				static constexpr booster_t boosters[ ] = {
					{ L"\uf0e7", L"Weekend Bonus", L"1.5x", { 245, 196, 66, 255 } },
					{ L"\uf06d", L"Win Streak",    L"+15%", { 150,  90, 255, 255 } },
				};

				for ( int i = 0; i < 2; ++i ) {
					const float bx = rates_x + 14.0f + i * ( boost_w + boost_gap );
					const auto& b = boosters[ i ];
					const auto boost_col = i == 1 ? col_accent : b.col;

					bypass::overlay::filled_rounded_rectangle(
						{ bx, boost_y, boost_w, boost_h }, 8.0f,
						bypass::overlay::color_t { boost_col.r, boost_col.g, boost_col.b, static_cast< std::uint8_t >( 22 ) } );
					bypass::overlay::rounded_rectangle(
						{ bx, boost_y, boost_w, boost_h }, 8.0f,
						bypass::overlay::color_t { boost_col.r, boost_col.g, boost_col.b, static_cast< std::uint8_t >( 80 ) }, 1.0f );

					bypass::overlay::text_ex( b.icon,
						{ bx + 8.0f, boost_y, 16.0f, boost_h },
						boost_col, 11.0f, bypass::overlay::text_weight_t::regular,
						L"Font Awesome", bypass::overlay::text_align_t::center, false );

					bypass::overlay::text_ex( b.label,
						{ bx + 26.0f, boost_y + 4.0f, boost_w - 34.0f, 12.0f },
						bypass::overlay::color_t { 210, 205, 220, 140 }, 9.0f,
						bypass::overlay::text_weight_t::semi_bold,
						L"Inter-Medium", bypass::overlay::text_align_t::left, false );

					bypass::overlay::text_ex( b.value,
						{ bx + 26.0f, boost_y + 16.0f, boost_w - 34.0f, 16.0f },
						boost_col, 12.0f, bypass::overlay::text_weight_t::black,
						L"Geist-Bold", bypass::overlay::text_align_t::left, false );
				}

				// Separator
				bypass::overlay::filled_rectangle(
					{ rates_x + rates_w * 0.45f, rates_y + 14.0f, 1.0f, rates_h - 28.0f },
					bypass::overlay::color_t { 255, 255, 255, 26 } );

				// How it works text
				struct economy_row_t { const wchar_t* icon; const wchar_t* text; };
				static constexpr economy_row_t economy_rows[ ] = {
					{ L"\uf05b", L"Kills and wins award XP" },
					{ L"\uf522", L"XP can be exchanged for Crisis Coins" },
					{ L"\uf080", L"Lifetime XP decides your scoreboard rank" },
					{ L"\uf466", L"Crates unlock features" },
					{ L"\uf073", L"Exchange XP for license time" },
				};
				const float rows_x = rates_x + rates_w * 0.45f + 10.0f;
				float economy_row_y = rates_y + 12.0f;

				for ( const auto& row : economy_rows ) {
					bypass::overlay::text_ex( row.icon,
						{ rows_x, economy_row_y, 16.0f, rate_row_h },
						bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 190 }, 10.0f,
						bypass::overlay::text_weight_t::regular,
						L"Font Awesome", bypass::overlay::text_align_t::center, false );

					bypass::overlay::text_ex( row.text,
						{ rows_x + 22.0f, economy_row_y, rates_w - ( rows_x - rates_x ) - 22.0f - 14.0f, rate_row_h },
						bypass::overlay::color_t { 210, 205, 220, 150 }, 10.5f,
						bypass::overlay::text_weight_t::bold,
						L"Inter-Bold", bypass::overlay::text_align_t::left, false );

					economy_row_y += rate_row_pitch;
				}

				// ── Exchange XP for License Time ──────────────────────────────────
				// -1 = "no pack chosen" — required so nothing looks
				// pre-selected on first render; the pack tiles below
				// only tint when the user actually clicks one, and
				// the EXTEND button stays disabled until then.
				static std::int32_t s_time_pack = -1;

				struct time_pack_t { const wchar_t* label; std::uint32_t hours; std::uint32_t xp_cost; };
				static constexpr time_pack_t time_packs[ ] = {
					{ L"3 Hours",  3,   8000 },
					{ L"24 Hours", 24,  40000 },
					{ L"1 Week",   168, 840000 },
				};

				const float buy_x = rates_x;
				const float buy_y = rates_y + rates_h + 12.0f;
				const float buy_w = rates_w;
				constexpr float buy_h = 116.0f;

				// Compact license purchase panel
				bypass::overlay::filled_rounded_rectangle(
					{ buy_x, buy_y, buy_w, buy_h }, 10.0f,
					bypass::overlay::color_t { 20, 17, 32, 175 } );
				bypass::overlay::rounded_rectangle(
					{ buy_x, buy_y, buy_w, buy_h }, 10.0f,
					col_border, 1.0f );

				bypass::overlay::text_ex( L"License Extension",
					{ buy_x + 14.0f, buy_y + 4.0f, buy_w - 28.0f, 24.0f },
					bypass::overlay::color_t { 247, 244, 253, 242 }, 13.0f,
					bypass::overlay::text_weight_t::black,
					L"Geist-Bold", bypass::overlay::text_align_t::center, false );

				constexpr int license_divider_segments = 64;
				for ( int i = 0; i < license_divider_segments; ++i ) {
					const float position = ( ( float ) i + 0.5f ) / license_divider_segments;
					float fade = ( std::min ) ( position, 1.0f - position ) / 0.38f;
					fade = ( std::clamp ) ( fade, 0.0f, 1.0f );
					fade = fade * fade * ( 3.0f - 2.0f * fade );
					const float x0 = buy_x + 14.0f + ( buy_w - 28.0f ) * ( float ) i / license_divider_segments;
					const float x1 = buy_x + 14.0f + ( buy_w - 28.0f ) * ( float ) ( i + 1 ) / license_divider_segments;
					bypass::overlay::filled_rectangle( { x0, buy_y + 31.0f, x1 - x0, 1.0f },
						bypass::overlay::color_t { 255, 255, 255,
							static_cast< std::uint8_t >( 24.0f * fade ) } );
				}

				// Duration offers
				constexpr float pack_gap = 6.0f;
				constexpr float buy_btn_w = 128.0f;
				const float pack_w = ( buy_w - 28.0f - buy_btn_w - pack_gap * 3.0f ) / 3.0f;
				constexpr float pack_h = 30.0f;
				const float pack_y = buy_y + 39.0f;

				for ( int i = 0; i < 3; ++i ) {
					const float pack_x = buy_x + 14.0f + static_cast< float >( i ) * ( pack_w + pack_gap );
					const bool pack_active = s_time_pack == static_cast< std::int32_t >( i );
					const bool pack_hovered = hit_test( mx, my, pack_x, pack_y, pack_w, pack_h );
					const bool affordable = xp >= time_packs[ i ].xp_cost;

					if ( clicked && pack_hovered && affordable )
						s_time_pack = static_cast< std::int32_t >( i );

					bypass::overlay::filled_rounded_rectangle(
						{ pack_x, pack_y, pack_w, pack_h }, 8.0f,
						pack_active
						? bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 30 }
						: bypass::overlay::color_t { 25, 22, 38,
							static_cast< std::uint8_t >( affordable ? ( pack_hovered ? 225u : 190u ) : 120u ) } );

					bypass::overlay::rounded_rectangle(
						{ pack_x, pack_y, pack_w, pack_h }, 8.0f,
						pack_active
						? bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 180 }
						: bypass::overlay::color_t { 255, 255, 255, static_cast< std::uint8_t >( affordable ? ( pack_hovered ? 34u : 18u ) : 10u ) },
						pack_active ? 1.5f : 1.0f );

					const wchar_t* compact_label = i == 0 ? L"3H" : i == 1 ? L"24H" : L"1W";
					bypass::overlay::text_ex( compact_label,
						{ pack_x + 4.0f, pack_y + 1.0f, pack_w - 8.0f, pack_h - 2.0f },
						affordable
						? ( pack_active
							? bypass::overlay::color_t { 248, 245, 255, 248 }
							: bypass::overlay::color_t { 218, 213, 232, 162 } )
						: bypass::overlay::color_t { 200, 195, 215, 68 },
						10.0f, bypass::overlay::text_weight_t::black,
						L"Geist-Bold", bypass::overlay::text_align_t::center, false );
				}

				// Action completes the compact offer row.
				// A pack must actually be selected before EXTEND can
				// price itself — sentinel -1 means "no selection yet",
				// which disables the button and shows a hint label
				// instead of a bogus 0 K price.
				constexpr float buy_btn_h = pack_h;
				const float buy_btn_x = buy_x + buy_w - buy_btn_w - 14.0f;
				const float buy_btn_y = pack_y;
				const bool buy_hovered = hit_test( mx, my, buy_btn_x, buy_btn_y, buy_btn_w, buy_btn_h );
				const bool pack_selected = s_time_pack >= 0
					&& s_time_pack < static_cast< std::int32_t >( std::size( time_packs ) );
				const bool buy_affordable = pack_selected
					&& xp >= time_packs[ s_time_pack ].xp_cost;
				wchar_t extend_text[ 32 ] {};
				if ( pack_selected )
					std::swprintf( extend_text, std::size( extend_text ), L"EXTEND FOR %.0fK",
						static_cast< double >( time_packs[ s_time_pack ].xp_cost ) / 1000.0 );
				else
					std::swprintf( extend_text, std::size( extend_text ), L"SELECT A PACK" );
				bypass::overlay::filled_rounded_rectangle(
					{ buy_btn_x, buy_btn_y, buy_btn_w, buy_btn_h }, buy_btn_h * 0.5f,
					buy_affordable
					? bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b,
						static_cast< std::uint8_t >( buy_hovered ? 235 : 200 ) }
				: bypass::overlay::color_t { 52, 48, 70, 145 } );
				bypass::overlay::rounded_rectangle(
					{ buy_btn_x, buy_btn_y, buy_btn_w, buy_btn_h }, buy_btn_h * 0.5f,
					buy_affordable
					? bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 235 }
				: bypass::overlay::color_t { 85, 80, 100, 100 }, 1.0f );
				bypass::overlay::text_ex( extend_text,
					{ buy_btn_x, buy_btn_y, buy_btn_w, buy_btn_h },
					buy_affordable ? bypass::overlay::color_t { 255, 255, 255, 245 }
					: bypass::overlay::color_t { 195, 190, 212, 95 }, 9.5f,
					bypass::overlay::text_weight_t::black, L"Geist-Bold",
					bypass::overlay::text_align_t::center, false );
				if ( clicked && buy_hovered && buy_affordable ) {
					if ( crisis::profile::client.redeem_time( time_packs[ s_time_pack ].hours ) )
						menu::notifications::push( L"License extended", time_packs[ s_time_pack ].label, L"\uf2f1",
								bypass::overlay::color_t { 61, 200, 122, 255 } );
					else
						menu::notifications::push( L"Extension failed", L"The server rejected the XP redemption.", L"\uf071",
								bypass::overlay::color_t { 220, 88, 88, 255 } );
				}

				// Purchase summary beneath the controls.
				// Falls back to 0 when no pack is selected so the
				// summary rows don't index a bogus time_pack — matches
				// the "SELECT A PACK" state above.
				const float buy_footer_y = pack_y + pack_h;
				const float buy_footer_h = buy_y + buy_h - buy_footer_y;
				const std::uint64_t selected_cost = pack_selected
					? time_packs[ s_time_pack ].xp_cost : 0ull;
				const std::uint64_t difference = pack_selected
					? ( buy_affordable ? xp - selected_cost : selected_cost - xp ) : 0ull;
				wchar_t balance_value[ 24 ] {};
				wchar_t cost_value[ 24 ] {};
				wchar_t result_value[ 24 ] {};
				std::swprintf( balance_value, std::size( balance_value ), L"%.1fK XP",
					static_cast< double >( xp ) / 1000.0 );
				std::swprintf( cost_value, std::size( cost_value ), L"%.0fK XP",
					static_cast< double >( selected_cost ) / 1000.0 );
				std::swprintf( result_value, std::size( result_value ), L"%.1fK XP",
					static_cast< double >( difference ) / 1000.0 );
				const float summary_x = buy_x + 14.0f;
				const float summary_w = buy_w - 28.0f;
				const float summary_col_w = summary_w / 3.0f;
				constexpr float summary_group_h = 29.0f;
				const float summary_y = buy_footer_y + ( buy_footer_h - summary_group_h ) * 0.5f;
				auto draw_summary = [ & ]( int column, const wchar_t* label, const wchar_t* value,
					bypass::overlay::color_t value_color ) {
						const float x = summary_x + summary_col_w * column;
						bypass::overlay::text_ex( label,
							{ x, summary_y, summary_col_w, 12.0f },
							bypass::overlay::color_t { 165, 161, 178, 165 }, 8.5f,
							bypass::overlay::text_weight_t::bold, L"Inter-Bold",
							bypass::overlay::text_align_t::center, false );
						bypass::overlay::text_ex( value,
							{ x, summary_y + 11.0f, summary_col_w, 18.0f },
							value_color, 11.0f, bypass::overlay::text_weight_t::bold, L"Inter-Bold",
							bypass::overlay::text_align_t::center, false );
					};
				draw_summary( 0, L"BALANCE", balance_value,
					bypass::overlay::color_t { 218, 213, 228, 210 } );
				draw_summary( 1, L"SELECTED COST", cost_value,
					bypass::overlay::color_t { 245, 196, 66, 235 } );
				draw_summary( 2, buy_affordable ? L"AFTER" : L"SHORTFALL", result_value,
					buy_affordable
					? bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 225 }
				: bypass::overlay::color_t { 235, 90, 100, 225 } );

				// ── Recent XP Earnings (Right Side) ───────────────────────────────
				const float right_gap = 12.0f;
				const float right_x = page_x + rates_w + right_gap;
				const float right_y = body_y;
				const float right_w = page_w - rates_w - right_gap;
				constexpr float right_h = 364.0f;

				bypass::overlay::filled_rounded_rectangle(
					{ right_x, right_y, right_w, right_h }, 10.0f,
					bypass::overlay::color_t { 22, 19, 35, 160 } );
				bypass::overlay::rounded_rectangle(
					{ right_x, right_y, right_w, right_h }, 10.0f, col_border, 1.0f );

				bypass::overlay::text_ex( L"RECENT XP EARNINGS",
					{ right_x + 14.0f, right_y + 14.0f, right_w - 28.0f, 16.0f },
					bypass::overlay::color_t { 213, 208, 225, 105 }, 10.0f,
					bypass::overlay::text_weight_t::semi_bold,
					L"Inter-Medium", bypass::overlay::text_align_t::left, false );

				bypass::overlay::filled_rectangle(
					{ right_x + 14.0f, right_y + 36.0f, right_w - 28.0f, 1.0f },
					bypass::overlay::color_t { 255, 255, 255, 22 } );

				constexpr float entry_h = 40.0f;
				constexpr float entry_gap = 4.0f;
				float entry_y = right_y + 46.0f;

				for ( const auto& earn : server_profile.recent_earnings ) {
					if ( entry_y + entry_h > right_y + right_h - 10.0f )
						break;

					const bool is_match_header = earn.event.find( L"Victory" ) != std::wstring::npos;
					const bool is_kill = earn.event.find( L"Elimination" ) != std::wstring::npos;
					const wchar_t* icon = is_match_header ? L"\uf091" : L"\uf05b";
					const std::uint8_t tr = is_kill ? 239 : 245;
					const std::uint8_t tg = is_kill ? 68 : 196;
					const std::uint8_t tb = is_kill ? 68 : 66;

					bypass::overlay::filled_rounded_rectangle(
						{ right_x + 12.0f, entry_y, right_w - 24.0f, entry_h }, 8.0f,
						is_match_header
						? bypass::overlay::color_t { 26, 23, 40, 180 }
					: bypass::overlay::color_t { 22, 19, 35, 130 } );
					bypass::overlay::rounded_rectangle(
						{ right_x + 12.0f, entry_y, right_w - 24.0f, entry_h }, 8.0f,
						is_match_header
						? bypass::overlay::color_t { tr, tg, tb, 40 }
					: bypass::overlay::color_t { 255, 255, 255, 12 }, 1.0f );

					bypass::overlay::text_ex( icon,
						{ right_x + 22.0f, entry_y, 20.0f, entry_h },
						bypass::overlay::color_t { tr, tg, tb,
							static_cast< std::uint8_t >( is_match_header ? 220 : 140 ) },
						is_match_header ? 12.0f : 10.0f,
						bypass::overlay::text_weight_t::regular,
						L"Font Awesome", bypass::overlay::text_align_t::center, false );

					bypass::overlay::text_ex( earn.event.c_str( ),
						{ right_x + 48.0f, entry_y + ( is_match_header ? 6.0f : 7.0f ),
						  140.0f, 14.0f },
						bypass::overlay::color_t { 244, 241, 250,
							static_cast< std::uint8_t >( is_match_header ? 230 : 200 ) },
						is_match_header ? 11.5f : 10.5f,
						bypass::overlay::text_weight_t::semi_bold,
						L"Geist-Medium", bypass::overlay::text_align_t::left, false );

					bypass::overlay::text_ex( earn.context.c_str( ),
						{ right_x + 48.0f, entry_y + ( is_match_header ? 21.0f : 22.0f ),
						  140.0f, 12.0f },
						bypass::overlay::color_t { 205, 200, 218, 95 }, 9.0f,
						bypass::overlay::text_weight_t::regular,
						L"Inter-Regular", bypass::overlay::text_align_t::left, false );

					wchar_t xp_buf[ 16 ] {};
					std::swprintf( xp_buf, std::size( xp_buf ), L"+%u", earn.amount );
					bypass::overlay::text_ex( xp_buf,
						{ right_x + 12.0f, entry_y + 14.0f, right_w - 32.0f, entry_h - 16.0f },
						bypass::overlay::color_t { tr, tg, tb,
							static_cast< std::uint8_t >( is_match_header ? 230 : 180 ) },
						is_match_header ? 13.0f : 11.5f,
						bypass::overlay::text_weight_t::black,
						L"Geist-Bold", bypass::overlay::text_align_t::right, false );

					entry_y += entry_h + entry_gap;
				}

				const float graph_gap = 12.0f;
				const float graph_y = body_y + 364.0f + graph_gap;
				const float graph_h = body_h - 364.0f - graph_gap;

				if ( graph_h > 40.0f ) {
					bypass::overlay::filled_rounded_rectangle(
						{ page_x, graph_y, page_w, graph_h }, 10.0f,
						bypass::overlay::color_t { 22, 19, 35, 160 } );

					bypass::overlay::rounded_rectangle(
						{ page_x, graph_y, page_w, graph_h }, 10.0f, col_border, 1.0f );

					bypass::overlay::text_ex( L"LIFETIME XP PROGRESSION",
						{ page_x + 14.0f, graph_y + 10.0f, 200.0f, 16.0f },
						bypass::overlay::color_t { 213, 208, 225, 105 }, 10.0f,
						bypass::overlay::text_weight_t::semi_bold,
						L"Inter-Medium", bypass::overlay::text_align_t::left, false );

					const float plot_x = page_x + 14.0f;
					const float plot_y = graph_y + 22.0f;
					const float plot_w = page_w - 28.0f;
					const float plot_h = graph_h - 42.0f;

					if ( plot_h > 10.0f ) {
						// Series comes from the auth server's per-session
						// lifetime XP snapshot. If auth is not connected
						// (server_profile.valid == false) or the series is
						// empty, render a FLAT baseline across the plot —
						// no fake curve, no phantom fill — so the graph
						// truthfully reads as "nothing recorded yet".
						const bool has_auth_series = server_profile.valid
							&& !server_profile.lifetime_progress.empty( );
						std::vector< float > xp_data;
						if ( has_auth_series ) {
							xp_data = server_profile.lifetime_progress;
							if ( xp_data.size( ) == 1 )
								xp_data.insert( xp_data.begin( ), xp_data.front( ) );
						}

						if ( !has_auth_series ) {
							// Straight-line fallback — draw the baseline
							// once, no fill, no vertices, no accent tint
							// that could imply activity. Sits at the
							// bottom of the plot (y = plot_y + plot_h).
							const float base_y = plot_y + plot_h;
							bypass::overlay::line(
								{ plot_x,          base_y },
								{ plot_x + plot_w, base_y },
								bypass::overlay::color_t { 150, 145, 170, 140 }, 1.5f );
						}
						else {
							const std::size_t data_points = xp_data.size( );

							float max_xp = 0.0f;
							for ( float v : xp_data ) if ( v > max_xp ) max_xp = v;
							if ( max_xp < 1.0f ) max_xp = 1.0f;

							std::vector< bypass::overlay::point_t > points( data_points );
							for ( std::size_t i = 0; i < data_points; ++i ) {
								points[ i ].x = plot_x + ( plot_w * i ) / ( data_points - 1 );
								points[ i ].y = plot_y + plot_h - ( plot_h * ( xp_data[ i ] / max_xp ) );
							}

							const float fill_bottom_limit = plot_y + plot_h + 14.0f;
							for ( std::size_t i = 0; i + 1 < data_points; ++i ) {
								float x = points[ i ].x;
								float y = points[ i ].y;
								float width = points[ i + 1 ].x - x;
								if ( width < 1.0f ) width = 1.0f;

								float height = fill_bottom_limit - y;
								if ( height > 0.0f ) {
									bypass::overlay::filled_rectangle(
										{ x, y, width, height },
										bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 35 }
									);
								}
							}

							for ( std::size_t i = 0; i + 1 < data_points; ++i ) {
								bypass::overlay::line(
									points[ i ], points[ i + 1 ],
									bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 255 }, 2.0f );
							}
						}
					}
				}
			}
			// ── Crates tab content ────────────────────────────────────────────────
			// ── Crates tab content ────────────────────────────────────────────────
			// ── Crates tab content ────────────────────────────────────────────────
							// ── Gamble tab content ────────────────────────────────────────────────
					// ── Gamble tab content ────────────────────────────────────────────────
					// ── Gamble tab content ────────────────────────────────────────────────
					// ── Gamble tab content ────────────────────────────────────────────────
					// ── Gamble tab content ────────────────────────────────────────────────
					// ── Gamble tab content ────────────────────────────────────────────────
			else if ( s_profile_page_tab == 1 )
			{
				const float body_y = tabs_y + tab_h + 12.0f;

				// ── Animation clock ───────────────────────────────────────────────
				const float animation_time = std::chrono::duration< float >(
					std::chrono::steady_clock::now( ).time_since_epoch( ) ).count( );
				static float s_gm_prev_time = 0.0f;
				const float gm_dt = ( s_gm_prev_time > 0.0f )
					? min( animation_time - s_gm_prev_time, 0.1f )
					: 0.016f;
				s_gm_prev_time = animation_time;

				// ── Game definitions ──────────────────────────────────────────────
				struct gamble_game_t {
					const wchar_t* icon;
					const wchar_t* name;
					const wchar_t* tagline;
					bypass::overlay::color_t col;
				};
				static constexpr gamble_game_t gamble_games[ ] = {
					{ L"\uf7d9", L"Mines",     L"Reveal safe tiles, avoid the mines",   { 245, 196,  66, 255 } },
					{ L"\uf110", L"Roulette",  L"Red, black or green",                  { 239,  68,  68, 255 } },
					{ L"\uf004", L"Blackjack", L"Beat the dealer to 21",                {  61, 200, 122, 255 } },
					{ L"\uf6d7", L"Chicken",   L"Push your luck before you get caught", { 249, 115,  22, 255 } },
					{ L"\uf201", L"Crash",     L"Ride the multiplier",                  { 150,  90, 255, 255 } },
				};
				constexpr int gamble_game_count = static_cast< int >( std::size( gamble_games ) );

				static std::uint32_t s_gamble_tab = 0;
				static float         s_gamble_anim_x = -1.0f;
				static bool          s_game_active = false;

				// ── Segmented game selector — Apple/Exodus sliding pill ───────────
				constexpr float gm_tab_h = 46.0f;
				constexpr float gm_tab_gap = 6.0f;
				constexpr float gm_tab_radius = 14.0f;
				constexpr float gm_tab_inset = 4.0f;
				const float gm_each_w = ( page_w - gm_tab_gap * ( gamble_game_count - 1 ) )
					/ static_cast< float >( gamble_game_count );

				bypass::overlay::filled_rounded_rectangle(
					{ page_x, body_y, page_w, gm_tab_h }, gm_tab_radius,
					bypass::overlay::color_t { 20, 18, 30, 235 } );

				const float gm_target_x = page_x
					+ static_cast< float >( s_gamble_tab ) * ( gm_each_w + gm_tab_gap );
				if ( s_gamble_anim_x < 0.0f )
					s_gamble_anim_x = gm_target_x;
				else
					s_gamble_anim_x += ( gm_target_x - s_gamble_anim_x ) * 0.18f;
				if ( std::abs( gm_target_x - s_gamble_anim_x ) < 0.5f )
					s_gamble_anim_x = gm_target_x;

				bypass::overlay::filled_rounded_rectangle(
					{ s_gamble_anim_x + gm_tab_inset, body_y + 2.0f,
					  gm_each_w - gm_tab_inset * 2.0f, gm_tab_h - 2.0f - gm_tab_inset },
					gm_tab_radius - gm_tab_inset,
					bypass::overlay::color_t { 48, 46, 55, 120 } );
				//bypass::overlay::rounded_rectangle(
				//	{ s_gamble_anim_x + gm_tab_inset, body_y + 2.0f,
				//	  gm_each_w - gm_tab_inset * 2.0f, gm_tab_h - 2.0f - gm_tab_inset },
				//	gm_tab_radius - gm_tab_inset,
				//	bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b,
				//		static_cast< std::uint8_t >( 55 ) }, 1.0f );

				for ( int i = 0; i < gamble_game_count; ++i ) {
					const float tab_x = page_x + static_cast< float >( i ) * ( gm_each_w + gm_tab_gap );
					const bool  hovered = hit_test( mx, my, tab_x, body_y, gm_each_w, gm_tab_h );
					const bool  active = s_gamble_tab == static_cast< std::uint32_t >( i );
					const auto& game = gamble_games[ i ];

					if ( clicked && hovered && !s_game_active )
						s_gamble_tab = static_cast< std::uint32_t >( i );

					constexpr float gm_icon_sz = 16.0f;
					constexpr float gm_lbl_gap = 7.0f;
					const float lbl_w = static_cast< float >( std::wcslen( game.name ) ) * 7.5f;
					const float group_w = gm_icon_sz + gm_lbl_gap + lbl_w;
					const float group_x = tab_x + ( gm_each_w - group_w ) * 0.5f;

					bypass::overlay::text_ex( game.icon,
						{ group_x, body_y, gm_icon_sz, gm_tab_h },
						active ? game.col
						: bypass::overlay::color_t { 215, 210, 228, static_cast< std::uint8_t >( hovered ? 175u : 95u ) },
						13.0f, bypass::overlay::text_weight_t::regular,
						L"Font Awesome", bypass::overlay::text_align_t::center, false );
					bypass::overlay::text_ex( game.name,
						{ group_x + gm_icon_sz + gm_lbl_gap, body_y, lbl_w, gm_tab_h },
						active ? bypass::overlay::color_t { 255, 255, 255, 255 }
						: bypass::overlay::color_t { 215, 210, 228, static_cast< std::uint8_t >( hovered ? 175u : 95u ) },
						12.5f, bypass::overlay::text_weight_t::semi_bold,
						L"Inter-Medium", bypass::overlay::text_align_t::left, false );
				}

				const auto& active_game = gamble_games[ s_gamble_tab ];

				// ── Per-game middle-control state ─────────────────────────────────
				static std::uint32_t s_coinflip_side = 0;
				static std::uint32_t s_roulette_pick = 0;
				static std::uint32_t s_chicken_risk = 1;
				static std::uint32_t s_crash_cashout = 1;

				// ── Shared game round state ───────────────────────────────────────
				static float         s_game_start = 0.0f;
				static float         s_game_bet = 0.0f;
				static int           s_game_result = 0;   // 0=pending, 1=win, -1=lose, 2=push
				static float         s_game_result_at = 0.0f;
				static std::uint32_t s_result_tab = 0;

				// ── Mines state ──────────────────────────────────────────────────
				static bool  s_mn_mines[ 25 ] = {};
				static bool  s_mn_revealed[ 25 ] = {};
				static int   s_mn_revealed_count = 0;
				static float s_mn_mult = 1.0f;
				static bool  s_mn_cashed_out = false;
				static float s_mn_reveal_at[ 25 ] = { -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f };
				static float s_mn_mult_pop_at = -10.0f;
				static float s_mn_explode_at = -10.0f;
				static float s_mn_explode_x = 0.0f;
				static float s_mn_explode_y = 0.0f;
				static float s_mn_shake_at = -10.0f;

				// ── Roulette state ────────────────────────────────────────────────
				static float s_rl_pos = 0.0f;
				static float s_rl_target = 0.0f;
				static float s_rl_start = 0.0f;
				static int   s_rl_result = 0;  // pocket index [0,13] that will land at pointer
				static bool  s_rl_spin = false;
				static float s_rl_ball_ang = -1.57079633f;
				static float s_rl_ball_rest = -1.57079633f;
				static float s_rl_spin_start = 0.0f;
				static bool  s_rl_result_pending = false;
				static float s_rl_win_mult = 1.0f;

				// ── Blackjack state ───────────────────────────────────────────────
				struct bj_card_t { std::uint8_t rank; std::uint8_t suit; }; // rank 1..13 (1=A,11=J,12=Q,13=K); suit 0=♠ 1=♥ 2=♦ 3=♣
				static bj_card_t s_bj_pcards[ 12 ];
				static int       s_bj_pcount = 0;
				static bj_card_t s_bj_dcards[ 12 ];
				static int       s_bj_dcount = 0;
				static int       s_bj_phase = 3;      // 0=deal-anim,1=player-turn,2=dealer-turn,3=idle
				static float     s_bj_step_at = 0.0f; // pacing timer for auto-hits / dealer draws
				static float     s_bj_pcard_at[ 12 ]; // per-card reveal timestamp, drives the deal/hit transition
				static float     s_bj_dcard_at[ 12 ];

				// ── Chicken state ─────────────────────────────────────────────────
				static int   s_ck_crossed = 0;
				static bool  s_ck_caught = false;
				static bool  s_ck_server_caught = false;
				static bool  s_ck_hopping = false;
				static float s_ck_hop_start = 0.0f;
				static float s_ck_hit_at = -10.0f;
				static int   s_ck_hit_lane = -1;
				static float s_ck_frozen_car_y = 0.0f;
				static float s_ck_hit_progress = 0.0f;
				static float s_ck_result_mult = 1.0f;
				static int   s_bj_result_pct = 0;

				// ── Crash state ───────────────────────────────────────────────────
				static float s_crash_mult = 1.0f;
				static float s_crash_point = 1.0f;
				static bool  s_crash_exploded = false;
				static float s_crash_cashout_m = 1.0f;
				static float s_crash_time_frac = 1.0f;
				static crisis::casino::result_t s_server_game;
				auto decode_card = [ ]( const std::string& card ) -> bj_card_t {
					if ( card.size( ) < 2 || card == "??" ) return { 0, 0 };
					const char suit = card.back( ); const std::string rank = card.substr( 0, card.size( ) - 1 );
					const std::uint8_t rank_value = rank == "A" ? 1 : rank == "J" ? 11 : rank == "Q" ? 12 : rank == "K" ? 13 : static_cast< std::uint8_t >( std::atoi( rank.c_str( ) ) );
					const std::uint8_t suit_value = suit == 'S' ? 0 : suit == 'H' ? 1 : suit == 'D' ? 2 : 3;
					return { rank_value, suit_value };
				};
				auto load_server_cards = [ & ]( const crisis::casino::result_t& result ) {
					s_bj_pcount = static_cast< int >( ( std::min )( result.player_cards.size( ), std::size( s_bj_pcards ) ) );
					s_bj_dcount = static_cast< int >( ( std::min )( result.dealer_cards.size( ), std::size( s_bj_dcards ) ) );
					for ( int i = 0; i < s_bj_pcount; ++i ) s_bj_pcards[ i ] = decode_card( result.player_cards[ i ] );
					for ( int i = 0; i < s_bj_dcount; ++i ) if ( result.dealer_cards[ i ] != "??" ) s_bj_dcards[ i ] = decode_card( result.dealer_cards[ i ] );
				};
				static bool s_active_round_restored = false;
				if ( !s_active_round_restored && crisis::casino::client.ready( ) ) {
					s_active_round_restored = true;
					bool restored = false;
					if ( const auto active = crisis::casino::client.active_round( "mines" ) ) {
						s_gamble_tab = 0;
						s_server_game = *active;
						s_game_active = true;
						s_game_start = animation_time;
						s_game_bet = static_cast< float >( active->bet );
						s_coinflip_side = active->mines == 5 ? 1u : active->mines == 8 ? 2u : 0u;
						std::memset( s_mn_mines, 0, sizeof( s_mn_mines ) );
						std::memset( s_mn_revealed, 0, sizeof( s_mn_revealed ) );
						for ( int i = 0; i < 25; ++i ) s_mn_reveal_at[ i ] = -100.0f;
						for ( const int tile : active->revealed ) if ( tile >= 0 && tile < 25 ) {
							s_mn_revealed[ tile ] = true;
							s_mn_reveal_at[ tile ] = animation_time - 1.0f;
						}
						s_mn_revealed_count = static_cast< int >( active->revealed.size( ) );
						s_mn_mult = static_cast< float >( active->multiplier );
						s_mn_cashed_out = false;
						restored = true;
					} else if ( const auto active = crisis::casino::client.active_round( "blackjack" ) ) {
						s_gamble_tab = 2;
						s_server_game = *active;
						s_game_active = true;
						s_game_start = animation_time;
						s_game_bet = static_cast< float >( active->bet );
						load_server_cards( *active );
						for ( int i = 0; i < s_bj_pcount; ++i ) s_bj_pcard_at[ i ] = animation_time - 1.0f;
						for ( int i = 0; i < s_bj_dcount; ++i ) s_bj_dcard_at[ i ] = animation_time - 1.0f;
						s_bj_phase = 1;
						restored = true;
					} else if ( const auto active = crisis::casino::client.active_round( "chicken" ) ) {
						s_gamble_tab = 3;
						s_server_game = *active;
						s_game_active = true;
						s_game_start = animation_time;
						s_game_bet = static_cast< float >( active->bet );
						s_chicken_risk = active->risk == "safe" ? 0u : active->risk == "wild" ? 2u : 1u;
						s_ck_crossed = active->crossed;
						s_ck_result_mult = static_cast< float >( active->multiplier );
						s_ck_caught = false;
						s_ck_server_caught = false;
						s_ck_hopping = false;
						restored = true;
					}
					if ( restored ) return;
				}

				// ── Arena layout ──────────────────────────────────────────────────
				constexpr float gm_bar_h = 48.0f;
				const float full_bot = ( content_y - 2.0f + panel_h ) - 20.0f;
				const float bar_y = full_bot - gm_bar_h;
				const float arena_x = page_x;
				const float arena_w = page_w;
				const float arena_y = body_y + gm_tab_h + 12.0f;
				const float arena_h = bar_y - arena_y - 12.0f;
				const float arena_cx = arena_x + arena_w * 0.5f;
				const float arena_cy = arena_y + arena_h * 0.5f;

				constexpr float rl_item_w = 64.0f;
				constexpr float rl_item_gap = 4.0f;
				constexpr float rl_per = rl_item_w + rl_item_gap;
				constexpr int   rl_seq_len = 37;

				// ── Arena background (same as crates) ─────────────────────────────
				bypass::overlay::filled_rounded_rectangle(
					{ arena_x, arena_y, arena_w, arena_h }, 12.0f,
					bypass::overlay::color_t { 22, 19, 35, 190 } );
				bypass::overlay::rounded_rectangle(
					{ arena_x, arena_y, arena_w, arena_h }, 12.0f, col_border, 1.0f );

				// ── Result overlay helper ─────────────────────────────────────────
				const float result_age = animation_time - s_game_result_at;
				const bool show_result = s_game_result != 0 && result_age < 2.5f;

				// ══════════════════════════════════════════════════════════════════
				// MINES
				// ══════════════════════════════════════════════════════════════════
				if ( s_gamble_tab == 0 )
				{
					constexpr int mn_cols = 5, mn_rows = 5;
					constexpr int mine_counts_arr[ 3 ] = { 3, 5, 8 };

					const int n_mines = mine_counts_arr[ s_coinflip_side ];

					const float tile_sz = ( ( arena_h < arena_w ? arena_h : arena_w ) - 32.0f ) / 5.0f - 4.0f;
					const float tile_pad = 4.0f;

					const float grid_w = tile_sz * mn_cols + tile_pad * ( mn_cols - 1 );
					const float grid_h = tile_sz * mn_rows + tile_pad * ( mn_rows - 1 );

					const float grid_x = arena_cx - grid_w * 0.5f;
					const float grid_y = arena_cy - grid_h * 0.5f;


					// Click handling
					if ( s_game_active && !s_mn_cashed_out ) {
						for ( int i = 0; i < 25; ++i ) {
							if ( s_mn_revealed[ i ] )
								continue;

							const int col = i % mn_cols, row = i / mn_cols;

							const float tx = grid_x + col * ( tile_sz + tile_pad );
							const float ty = grid_y + row * ( tile_sz + tile_pad );

							if ( clicked && hit_test( mx, my, tx, ty, tile_sz, tile_sz ) ) {
								s_server_game = crisis::casino::client.mines_reveal( i );
								if ( apply_casino_result( s_server_game ) ) {
									s_mn_revealed[ i ] = true; s_mn_reveal_at[ i ] = animation_time;
									s_mn_revealed_count = static_cast< int >( s_server_game.revealed.size( ) );
									s_mn_mult = static_cast< float >( s_server_game.multiplier ); s_mn_mult_pop_at = animation_time;
									if ( s_server_game.settled( ) ) {
										std::memset( s_mn_mines, 0, sizeof( s_mn_mines ) );
										for ( const int mine : s_server_game.mine_positions ) if ( mine >= 0 && mine < 25 ) s_mn_mines[ mine ] = true;
										int order = 0; for ( int j = 0; j < 25; ++j ) { s_mn_revealed[ j ] = true; if ( j != i ) s_mn_reveal_at[ j ] = animation_time + 0.12f + order++ * 0.05f; }
										s_game_active = false; s_game_result = s_server_game.status == "lost" ? -1 : 1; s_game_result_at = animation_time; s_result_tab = s_gamble_tab;
										if ( s_game_result < 0 ) { s_mn_shake_at = animation_time; s_shake_at = animation_time; s_mn_explode_at = animation_time; s_mn_explode_x = tx + tile_sz * 0.5f; s_mn_explode_y = ty + tile_sz * 0.5f; }
									}
								}

								break;
							}
						}
					}

					// Draw tiles — animated
					for ( int i = 0; i < 25; ++i ) {
						const int col = i % mn_cols, row = i / mn_cols;

						const float tx = grid_x + col * ( tile_sz + tile_pad );
						const float ty = grid_y + row * ( tile_sz + tile_pad );

						const bool is_mine = s_mn_mines[ i ];
						const bool rev = s_mn_revealed[ i ];
						const bool hov = s_game_active && !s_mn_cashed_out && !rev
							&& hit_test( mx, my, tx, ty, tile_sz, tile_sz );

						const float raw_t = ( animation_time - s_mn_reveal_at[ i ] ) / 0.3f;
						const bool anim_rev = rev && raw_t > 0.0f;

						const float pop_s = anim_rev
							? ( ( raw_t < 1.0f )
								? ( raw_t < 0.5f ? raw_t * 1.2f : 1.2f - ( raw_t - 0.5f ) * 0.4f )
								: 1.0f )
							: 1.0f;

						const float dsz_w = tile_sz * pop_s;
						const float dsz_h = tile_sz * pop_s;

						const float dtx = tx + ( tile_sz - dsz_w ) * 0.5f;
						const float dty = ty + ( tile_sz - dsz_h ) * 0.5f;

						const bypass::overlay::color_t tile_col = anim_rev
							? ( is_mine ? bypass::overlay::color_t { 180, 42, 42, 255 }
								: bypass::overlay::color_t { 38, 158, 76, 255 } )
							: ( hov ? bypass::overlay::color_t { 72, 66, 92, 255 }
						: bypass::overlay::color_t { 46, 42, 60, 255 } );

						bypass::overlay::filled_rounded_rectangle(
							{ dtx, dty, dsz_w, dsz_h }, 5.0f, tile_col );

						if ( anim_rev && is_mine ) {
							const float ic = dsz_h * 0.55f;

							bypass::overlay::text_ex(
								L"\xF1E2",
								{ dtx + dsz_w * 0.5f - ic * 0.5f,
								  dty + dsz_h * 0.5f - ic * 0.5f,
								  ic, ic },
								bypass::overlay::color_t { 255, 210, 210, 255 },
								ic * 0.85f,
								bypass::overlay::text_weight_t::bold,
								L"Font Awesome",
								bypass::overlay::text_align_t::center,
								false );
						}
						else if ( hov ) {
							bypass::overlay::filled_rounded_rectangle(
								{ tx, ty, tile_sz, 3.0f }, 1.5f,
								bypass::overlay::color_t { 200, 190, 255, 80 } );
						}
					}

					// Explosion rings
					{
						const float et = animation_time - s_mn_explode_at;

						if ( et >= 0.0f && et < 1.0f )
						{
							bypass::overlay::filled_circle(
								{ s_mn_explode_x, s_mn_explode_y }, et * 55.0f,
								bypass::overlay::color_t { 255, 140, 40, static_cast< std::uint8_t >( ( 1.0f - et ) * 200.0f ) } );

							if ( et > 0.18f )
							{
								const float et2 = et - 0.18f;

								bypass::overlay::filled_circle(
									{ s_mn_explode_x, s_mn_explode_y }, et2 * 72.0f,
									bypass::overlay::color_t { 255, 80, 20, static_cast< std::uint8_t >( ( 1.0f - et2 / 0.82f ) * 140.0f ) } );
							}
						}
					}

					// Progress bar under the Mines board
					{
						int revealed_safe = 0;

						for ( int j = 0; j < 25; ++j ) {
							const bool visually_revealed =
								s_mn_revealed[ j ] && animation_time >= s_mn_reveal_at[ j ];

							if ( !s_mn_mines[ j ] && visually_revealed )
								++revealed_safe;
						}

						const int total_safe = 25 - n_mines;

						const float progress = total_safe > 0
							? ( std::clamp ) (
								static_cast< float >( revealed_safe ) / static_cast< float >( total_safe ),
								0.0f, 1.0f )
							: 0.0f;

						const float bar_w = grid_w;
						const float bar_h = 6.0f;
						const float bar_x = grid_x;
						const float bar_y = grid_y + grid_h + 10.0f;

						if ( bar_y + bar_h <= arena_y + arena_h - 6.0f ) {
							bypass::overlay::filled_rounded_rectangle(
								{ bar_x, bar_y, bar_w, bar_h }, bar_h * 0.5f,
								bypass::overlay::color_t { 255, 255, 255, 26 } );

							if ( progress > 0.0f ) {
								bypass::overlay::filled_rounded_rectangle(
									{ bar_x, bar_y, ( std::max ) ( bar_h, bar_w * progress ), bar_h },
									bar_h * 0.5f,
									bypass::overlay::color_t { 61, 200, 122, 225 } );
							}
						}
					}

					// Left panel — multiplier
					{
						const float lw = ( grid_x - arena_x ) - 4.0f;
						const float lx = arena_x + 2.0f;
						const float cy = arena_cy;
						const float pop_t2 = ( animation_time - s_mn_mult_pop_at ) / 0.4f;
						const float pop_s2 = ( pop_t2 < 1.0f ) ? ( 1.0f + 0.35f * std::sin( pop_t2 * 3.14159f ) ) : 1.0f;
						const bool show_m = s_mn_revealed_count > 0 || s_game_result != 0;

						wchar_t mult_buf[ 48 ];
						if ( s_game_result == 1 )
							std::swprintf( mult_buf, 48, L"You won \xD7%.2f!", s_mn_mult );
						else
							std::swprintf( mult_buf, 48, L"\xD7%.2f", s_mn_mult );

						// Coin Icon — 2.3x bigger (11.0 * 2.3 = 25.3), moved up slightly to align with bigger text
						bypass::overlay::text_ex( L"\xF51E",
							{ lx, cy - 28.0f, lw, 25.3f },
							bypass::overlay::color_t { 245, 196, 66, static_cast< std::uint8_t >( show_m ? 200u : 60u ) }, 25.3f,
							bypass::overlay::text_weight_t::bold, L"Font Awesome",
							bypass::overlay::text_align_t::center, false );

						// Multiplier Value — Bigger (16.0f)
						bypass::overlay::text_ex( mult_buf,
							{ lx, cy - 2.0f, lw, 20.0f },
							bypass::overlay::color_t { 245, 220, 100, static_cast< std::uint8_t >( show_m ? 230u : 60u ) },
							16.0f * pop_s2, bypass::overlay::text_weight_t::black, L"Geist-Bold",
							bypass::overlay::text_align_t::center, false );

						// Separator — Thicker (2.0f) and moved down (cy + 22.0f) for more space
						if ( show_m ) {
							bypass::overlay::filled_rounded_rectangle(
								{ lx + lw * 0.2f, cy + 22.0f, lw * 0.6f, 2.0f }, 1.0f,
								bypass::overlay::color_t { 255, 255, 255, 40 } );

							// Payout Text — Bigger (14.0f) and moved down (cy + 28.0f)
							const std::uint32_t payout_val = static_cast< std::uint32_t >( s_game_bet * s_mn_mult );
							wchar_t pay_buf[ 24 ];
							std::swprintf( pay_buf, 24, L"+%u XP", payout_val );

							bypass::overlay::text_ex( pay_buf,
								{ lx, cy + 28.0f, lw, 18.0f },
								bypass::overlay::color_t { 170, 120, 255, 235 }, 14.0f,
								bypass::overlay::text_weight_t::black, L"Geist-Bold",
								bypass::overlay::text_align_t::center, false );
						}
					}
					// Right panel — remaining safe tiles
					{
						const float rx = grid_x + grid_w + 4.0f;
						const float rw = ( arena_x + arena_w ) - rx - 2.0f;
						const float cy = arena_cy;
						const int rem_s = ( 25 - n_mines ) - s_mn_revealed_count;

						// Check Icon (Slightly bigger, less transparent)
						bypass::overlay::text_ex( L"\xF058",
							{ rx, cy - 24.0f, rw, 16.0f },
							bypass::overlay::color_t { 100, 200, 255, 235 }, 13.0f,
							bypass::overlay::text_weight_t::bold, L"Font Awesome",
							bypass::overlay::text_align_t::center, false );

						wchar_t rem_buf[ 8 ];
						std::swprintf( rem_buf, 8, L"%d", ( std::max ) ( rem_s, 0 ) );

						// Remaining Count (Bigger, less transparent)
						bypass::overlay::text_ex( rem_buf,
							{ rx, cy - 4.0f, rw, 20.0f },
							bypass::overlay::color_t { 100, 200, 255, 255 },
							16.0f, bypass::overlay::text_weight_t::black, L"Geist-Bold",
							bypass::overlay::text_align_t::center, false );
					}
				}
				// ══════════════════════════════════════════════════════════════════
								// ROULETTE
								// ══════════════════════════════════════════════════════════════════
				else if ( s_gamble_tab == 1 )
				{
					// Standard single-zero European wheel colors for pockets 0-36.
					static constexpr int rl_seq[ ] = { 2,0,1,0,1,0,1,0,1,0,1,1,0,1,0,1,0,1,0,0,1,0,1,0,1,0,1,0,1,1,0,1,0,1,0,1,0 };
					const bypass::overlay::color_t rl_cols[ ] = {
						{ 239, 68, 68, 255 }, { 40, 40, 48, 255 }, { 34, 197, 94, 255 }
					};

					// Spin physics — drives s_rl_pos toward s_rl_target
					if ( s_rl_spin ) {
						s_rl_pos += ( s_rl_target - s_rl_pos ) * gm_dt * 4.5f;
						if ( std::abs( s_rl_target - s_rl_pos ) < 0.5f ) {
							s_rl_pos = s_rl_target;
							s_rl_spin = false;
							s_rl_result_pending = true;
						}
					}
					else if ( !s_game_active && ( s_game_result == 0 || result_age > 2.5f ) ) {
						s_rl_pos += 30.0f * gm_dt; // idle scroll — only when no result is being shown
					}

					// ── Standard European single-zero wheel ─────────────────────────
					constexpr int num_pockets = 37;
					constexpr float wedge = 2.0f * 3.14159265f / num_pockets;
					constexpr float pointer_angle = -1.57079633f; // top / 12 o'clock
					const float center_offset = ( arena_w - rl_item_w ) * 0.5f;
					const float wheel_phase = ( s_rl_pos + center_offset ) / rl_per;

					// ── Resolve result from ball angle when spin just stopped ──
					if ( s_rl_result_pending ) {
						s_rl_result_pending = false;
						s_game_active = false;
						const float ball_rel = s_rl_ball_ang - pointer_angle;
						const float raw_idx = wheel_phase + ball_rel / wedge;
						s_rl_result = ( ( static_cast< int >( std::round( raw_idx ) ) % num_pockets ) + num_pockets ) % num_pockets;
						s_rl_win_mult = s_server_game.won ? static_cast< float >( s_server_game.payout ) / ( std::max )( s_game_bet, 1.0f ) : 0.0f;
						s_game_result = s_server_game.won ? 1 : -1;
						s_game_result_at = animation_time;
						s_result_tab = s_gamble_tab;
					}

					const float wheel_cx = arena_cx;
					const float wheel_cy = arena_cy;
					const float wheel_r = ( std::min ) ( arena_h, arena_w ) * 0.36f;
					const float hub_r = wheel_r * 0.34f;
					const float rim_r = wheel_r * 1.05f;
					const float ball_r = wheel_r * 1.13f;
					const float track_r = wheel_r * 1.22f;

					auto pocket_angle = [ & ]( float idx ) {
						return pointer_angle + ( idx - wheel_phase ) * wedge;
						};

					// ── Outer decorative tick track — slow ambient rotation ────────
					{
						constexpr int track_ticks = num_pockets * 2;
						const float    track_phase = -animation_time * 0.15f;
						for ( int i = 0; i < track_ticks; ++i ) {
							const float a = track_phase + ( static_cast< float >( i ) / track_ticks ) * 3.14159265f * 2.0f;
							const float x0 = wheel_cx + std::cos( a ) * track_r * 0.92f;
							const float y0 = wheel_cy + std::sin( a ) * track_r * 0.92f;
							const float x1 = wheel_cx + std::cos( a ) * track_r;
							const float y1 = wheel_cy + std::sin( a ) * track_r;
							bypass::overlay::line( { x0, y0 }, { x1, y1 },
								bypass::overlay::color_t { 140, 135, 160, 38 }, 1.5f );
						}
					}

					// ── Rim ────────────────────────────────────────────────────────
					bypass::overlay::filled_circle( { wheel_cx, wheel_cy }, rim_r,
						bypass::overlay::color_t { 178, 134, 34, 255 } );

					// ── Pockets — pie wedges approximated with radiating thick lines ──
					constexpr int sub_lines = 12;
					const float   sub_thick = ( wheel_r * wedge / sub_lines ) * 1.2f;
					for ( int i = 0; i < num_pockets; ++i ) {
						const float base = pocket_angle( static_cast< float >( i ) );
						const auto& col = rl_cols[ rl_seq[ i ] ];
						for ( int k = 0; k < sub_lines; ++k ) {
							const float a = base - wedge * 0.5f + ( static_cast< float >( k ) + 0.5f ) / sub_lines * wedge;
							const float x0 = wheel_cx + std::cos( a ) * hub_r;
							const float y0 = wheel_cy + std::sin( a ) * hub_r;
							const float x1 = wheel_cx + std::cos( a ) * wheel_r;
							const float y1 = wheel_cy + std::sin( a ) * wheel_r;
							bypass::overlay::line( { x0, y0 }, { x1, y1 }, col, sub_thick );
						}
					}

					// ── Hub ────────────────────────────────────────────────────────
					bypass::overlay::filled_circle( { wheel_cx, wheel_cy }, hub_r,
						bypass::overlay::color_t { 178, 134, 34, 255 } );
					bypass::overlay::filled_circle( { wheel_cx, wheel_cy }, hub_r * 0.86f,
						bypass::overlay::color_t { 30, 27, 38, 255 } );
					// Hub center — Font Awesome star icon
					{
						const float pulse = 0.92f + 0.08f * std::sin( animation_time * ( s_rl_spin ? 6.0f : 1.2f ) );
						bypass::overlay::text_ex( L"\xF005",
							{ wheel_cx - hub_r * 0.72f * pulse, wheel_cy - hub_r * 0.72f * pulse,
							  hub_r * 1.44f * pulse, hub_r * 1.44f * pulse },
							bypass::overlay::color_t { 245, 196, 66, 230 },
							hub_r * 0.80f * pulse, bypass::overlay::text_weight_t::regular,
							L"Font Awesome", bypass::overlay::text_align_t::center, false );
					}

					// ── Ball — ease-in spin, then settle on winning pocket ──────────────
					if ( s_rl_spin ) {
						const float remain = std::abs( s_rl_target - s_rl_pos );
						const float decel_t = ( std::min ) ( remain / 8.0f, 1.0f );
						const float accel_t = ( std::min ) ( ( animation_time - s_rl_spin_start ) / 1.8f, 1.0f );
						const float speed = ( 14.0f * decel_t * decel_t * decel_t + 0.1f ) * accel_t;
						s_rl_ball_ang += speed * gm_dt;
					}
					else { /* ball frozen at current angle when not spinning */ }
					{
						const float bx = wheel_cx + std::cos( s_rl_ball_ang ) * ball_r;
						const float by = wheel_cy + std::sin( s_rl_ball_ang ) * ball_r;
						bypass::overlay::filled_circle( { bx, by + 1.5f }, 5.0f,
							bypass::overlay::color_t { 0, 0, 0, 90 } );
						bypass::overlay::filled_circle( { bx, by }, 5.0f,
							bypass::overlay::color_t { 235, 233, 240, 255 } );
						bypass::overlay::filled_circle( { bx - 1.3f, by - 1.3f }, 1.6f,
							bypass::overlay::color_t { 255, 255, 255, 220 } );
					}
				}
				// ══════════════════════════════════════════════════════════════════
				// BLACKJACK
				// ══════════════════════════════════════════════════════════════════
				else if ( s_gamble_tab == 2 )
				{
					const float elapsed = animation_time - s_game_start;
					constexpr float deal_dur = 1.7f;
					constexpr float card_anim_dur = 0.4f;

					// ── Card + hand helpers ───────────────────────────────────────
					auto bj_rank_str = [ ]( int rank, wchar_t* buf ) {
						if ( rank == 1 ) { buf[ 0 ] = L'A'; buf[ 1 ] = 0; }
						else if ( rank == 10 ) { buf[ 0 ] = L'1'; buf[ 1 ] = L'0'; buf[ 2 ] = 0; }
						else if ( rank == 11 ) { buf[ 0 ] = L'J'; buf[ 1 ] = 0; }
						else if ( rank == 12 ) { buf[ 0 ] = L'Q'; buf[ 1 ] = 0; }
						else if ( rank == 13 ) { buf[ 0 ] = L'K'; buf[ 1 ] = 0; }
						else { buf[ 0 ] = static_cast< wchar_t >( L'0' + rank ); buf[ 1 ] = 0; }
						};
					auto bj_suit_col = [ ]( int suit ) -> bypass::overlay::color_t {
						return ( suit == 1 || suit == 2 )
							? bypass::overlay::color_t { 199, 18, 46, 255 }
						: bypass::overlay::color_t { 23, 20, 26, 255 };
						};
					// Hearts/diamonds use the bundled Font Awesome glyphs; spades/clubs
					// aren't in that font's free tier, so they're hand-built from primitives.
					// All four suits are hand-built from primitives — no Font Awesome
					// involved anywhere in card rendering.
					auto bj_draw_suit = [ & ]( float cx, float cy, float size, int suit, bypass::overlay::color_t col ) {
						if ( suit < 0 || suit > 3 ) return;
						const float r = size * 0.46f;
						if ( suit == 1 ) {
							// Heart: two circles + bottom converging lines
							const float cr = r * 0.60f;
							const float ox = r * 0.52f;
							bypass::overlay::filled_circle( { cx - ox, cy - cr * 0.18f }, cr, col );
							bypass::overlay::filled_circle( { cx + ox, cy - cr * 0.18f }, cr, col );
							bypass::overlay::line( { cx - r, cy + cr * 0.22f }, { cx, cy + r }, col, r * 0.95f );
							bypass::overlay::line( { cx + r, cy + cr * 0.22f }, { cx, cy + r }, col, r * 0.95f );
						}
						else if ( suit == 2 ) {
							// Diamond: four thick lines forming filled diamond
							const float lw = r * 1.42f;
							bypass::overlay::line( { cx, cy - r }, { cx + r, cy }, col, lw );
							bypass::overlay::line( { cx, cy - r }, { cx - r, cy }, col, lw );
							bypass::overlay::line( { cx, cy + r }, { cx + r, cy }, col, lw );
							bypass::overlay::line( { cx, cy + r }, { cx - r, cy }, col, lw );
						}
						else if ( suit == 0 ) {
							// Spade: upward pointing body + two small base circles + stem
							bypass::overlay::line( { cx - r, cy + r * 0.08f }, { cx, cy - r }, col, r * 0.95f );
							bypass::overlay::line( { cx + r, cy + r * 0.08f }, { cx, cy - r }, col, r * 0.95f );
							bypass::overlay::filled_circle( { cx, cy - r * 0.15f }, r * 0.58f, col );
							bypass::overlay::filled_circle( { cx - r * 0.50f, cy + r * 0.42f }, r * 0.36f, col );
							bypass::overlay::filled_circle( { cx + r * 0.50f, cy + r * 0.42f }, r * 0.36f, col );
							bypass::overlay::filled_rectangle( { cx - r * 0.16f, cy + r * 0.60f, r * 0.32f, r * 0.46f }, col );
						}
						else {
							// Club: three filled circles + stem
							bypass::overlay::filled_circle( { cx,            cy - r * 0.40f }, r * 0.44f, col );
							bypass::overlay::filled_circle( { cx - r * 0.44f, cy + r * 0.14f }, r * 0.44f, col );
							bypass::overlay::filled_circle( { cx + r * 0.44f, cy + r * 0.14f }, r * 0.44f, col );
							bypass::overlay::filled_rectangle( { cx - r * 0.16f, cy + r * 0.50f, r * 0.32f, r * 0.46f }, col );
						}
						};
					auto bj_draw_card = [ & ]( float cx, float cy, float cw, float ch, bj_card_t card, float alpha_t, bool face_down ) {
						if ( alpha_t <= 0.005f ) return;
						const std::uint8_t a = static_cast< std::uint8_t >( alpha_t * 255 );
						if ( face_down ) {
							// 1. Base Card (Deep Casino Red)
							bypass::overlay::filled_rounded_rectangle( { cx, cy, cw, ch }, 6.0f,
								bypass::overlay::color_t { 115, 18, 28, a } );

							// 2. Inner Gold Border
							bypass::overlay::rounded_rectangle( { cx + 4.0f, cy + 4.0f, cw - 8.0f, ch - 8.0f }, 4.0f,
								bypass::overlay::color_t { 210, 180, 100, static_cast< std::uint8_t >( alpha_t * 120 ) }, 2.5f );

							// 3. Dark Outer Edge
							bypass::overlay::rounded_rectangle( { cx, cy, cw, ch }, 6.0f,
								bypass::overlay::color_t { 45, 8, 12, static_cast< std::uint8_t >( alpha_t * 220 ) }, 1.5f );

							// 4. Center Emblem Setup
							const float em_cx = cx + cw * 0.5f;
							const float em_cy = cy + ch * 0.5f;
							const float em_r = ch * 0.20f;

							// [ OPTION A: DICE ICON ] - Perfect for a gambling game (Active)
							bypass::overlay::text_ex( L"\xF522",
								{ em_cx - em_r, em_cy - em_r, em_r * 2.0f, em_r * 2.0f },
								bypass::overlay::color_t { 235, 205, 120, static_cast< std::uint8_t >( alpha_t * 210 ) }, em_r * 1.4f,
								bypass::overlay::text_weight_t::regular, L"Font Awesome",
								bypass::overlay::text_align_t::center, false );

							/* --- ALTERNATIVE EMBLEMS (Uncomment to use, delete Option A) ---

							// [ OPTION B: CROWN ICON ] - High roller / VIP feel
							bypass::overlay::text_ex( L"\xF521",
								{ em_cx - em_r, em_cy - em_r, em_r * 2.0f, em_r * 2.0f },
								bypass::overlay::color_t { 235, 205, 120, static_cast< std::uint8_t >( alpha_t * 210 ) }, em_r * 1.4f,
								bypass::overlay::text_weight_t::regular, L"Font Awesome",
								bypass::overlay::text_align_t::center, false );

							// [ OPTION C: CUSTOM GEOMETRIC CREST ] - Sleek & modern
							bypass::overlay::line( { em_cx, em_cy - em_r }, { em_cx + em_r * 0.75f, em_cy },
								bypass::overlay::color_t { 235, 205, 120, static_cast< std::uint8_t >( alpha_t * 180 ) }, 2.5f );
							bypass::overlay::line( { em_cx + em_r * 0.75f, em_cy }, { em_cx, em_cy + em_r },
								bypass::overlay::color_t { 235, 205, 120, static_cast< std::uint8_t >( alpha_t * 180 ) }, 2.5f );
							bypass::overlay::line( { em_cx, em_cy + em_r }, { em_cx - em_r * 0.75f, em_cy },
								bypass::overlay::color_t { 235, 205, 120, static_cast< std::uint8_t >( alpha_t * 180 ) }, 2.5f );
							bypass::overlay::line( { em_cx - em_r * 0.75f, em_cy }, { em_cx, em_cy - em_r },
								bypass::overlay::color_t { 235, 205, 120, static_cast< std::uint8_t >( alpha_t * 180 ) }, 2.5f );
							bypass::overlay::filled_circle( { em_cx, em_cy }, em_r * 0.25f,
								bypass::overlay::color_t { 245, 196, 66, static_cast< std::uint8_t >( alpha_t * 220 ) } );
							*/

							return;
						}
						bypass::overlay::filled_rounded_rectangle( { cx, cy, cw, ch }, 6.0f,
							bypass::overlay::color_t { 250, 250, 253, a } );
						bypass::overlay::rounded_rectangle( { cx, cy, cw, ch }, 6.0f,
							bypass::overlay::color_t { 20, 16, 30, static_cast< std::uint8_t >( alpha_t * 90 ) }, 1.0f );

						const auto base_col = bj_suit_col( card.suit );
						const bypass::overlay::color_t col_a { base_col.r, base_col.g, base_col.b, a };

						wchar_t rank_buf[ 4 ];
						bj_rank_str( card.rank, rank_buf );

						bypass::overlay::text_ex( rank_buf,
							{ cx + 4.0f, cy + 3.0f, cw * 0.5f, ch * 0.17f }, col_a, 12.0f,
							bypass::overlay::text_weight_t::bold, L"Geist-Bold",
							bypass::overlay::text_align_t::left, false );
						bj_draw_suit( cx + cw * 0.15f, cy + ch * 0.30f, ch * 0.07f, card.suit, col_a );

						bypass::overlay::text_ex( rank_buf,
							{ cx + cw * 0.5f - 4.0f, cy + ch - ch * 0.20f, cw * 0.5f - 4.0f, ch * 0.17f }, col_a, 12.0f,
							bypass::overlay::text_weight_t::bold, L"Geist-Bold",
							bypass::overlay::text_align_t::right, false );
						bj_draw_suit( cx + cw * 0.85f, cy + ch * 0.70f, ch * 0.07f, card.suit, col_a );

						bj_draw_suit( cx + cw * 0.5f, cy + ch * 0.52f, ch * 0.20f, card.suit, col_a );
						};
					auto bj_hand_value = [ ]( const bj_card_t* cards, int count, bool* soft ) -> int {
						int total = 0, aces = 0;
						for ( int i = 0; i < count; ++i ) {
							const int r = cards[ i ].rank;
							total += ( r == 1 ) ? 11 : ( r >= 10 ? 10 : r );
							if ( r == 1 ) ++aces;
						}
						while ( total > 21 && aces > 0 ) { total -= 10; --aces; }
						if ( soft ) *soft = aces > 0;
						return total;
						};
					if ( s_bj_pcount == 0 ) {
						bypass::overlay::text_ex( L"Place a bet to deal",
							{ arena_x, arena_cy - 10.0f, arena_w, 20.0f },
							bypass::overlay::color_t { 140, 134, 165, 180 }, 12.0f,
							bypass::overlay::text_weight_t::bold, L"Inter-Bold",
							bypass::overlay::text_align_t::center, false );
					}
					else {
						// ── Phase progression ────────────────────────────────────
						if ( s_bj_phase == 0 ) {
							if ( elapsed >= deal_dur ) {
								bool nsoft = false;
								const int nv = bj_hand_value( s_bj_pcards, s_bj_pcount, &nsoft );
								if ( nv == 21 ) { s_bj_phase = 2; s_bj_step_at = animation_time + 1.0f; }
								else { s_bj_phase = 1; }
							}
						}
						else if ( s_bj_phase == 2 && animation_time >= s_bj_step_at ) {
							s_game_active = false;
							s_game_result = s_server_game.status == "push" ? 2 : s_server_game.status == "won" || s_server_game.status == "blackjack" ? 1 : -1;
							s_bj_result_pct = s_game_result == 1 ? 75 : s_game_result == 2 ? 50 : 25;
							s_game_result_at = animation_time + card_anim_dur; s_result_tab = s_gamble_tab; s_bj_phase = 3;
						}

						// ── Layout: dealer left-center, player right-center ──────────
						const float card_h = ( std::min ) ( arena_h * 0.46f, 130.0f );
						const float card_w = card_h * 0.68f;
						const float card_overlap = card_w * 0.38f;
						const float dealer_base_cx = arena_x + arena_w * 0.25f;
						const float player_base_cx = arena_x + arena_w * 0.75f;
						const float card_y = arena_cy - card_h * 0.5f;
						const float deck_draw_cx = arena_cx - card_w * 0.5f;
						const float deck_draw_cy = arena_cy - card_h * 0.5f;
						const bool  hole_hidden = s_bj_phase <= 1;

						bool p_soft = false, d_soft = false;
						const int p_val = bj_hand_value( s_bj_pcards, s_bj_pcount, &p_soft );
						const int d_val = bj_hand_value( s_bj_dcards, s_bj_dcount, &d_soft );

						auto card_alpha = [ & ]( float added_at ) -> float {
							return ( std::max ) ( 0.0f, ( std::min ) ( ( animation_time - added_at ) / card_anim_dur, 1.0f ) );
							};

						// ── Deck visual — stacked card backs at center ─────────────
						{
							constexpr int deck_depth = 5;
							for ( int d = deck_depth; d >= 0; --d ) {
								const float dx = deck_draw_cx + d * 0.7f;
								const float dy = deck_draw_cy - d * 0.7f;

								// Casino red base
								bypass::overlay::filled_rounded_rectangle( { dx, dy, card_w, card_h }, 4.0f,
									bypass::overlay::color_t { 125, 22, 32, 255 } );

								// Dark red outer edge
								bypass::overlay::rounded_rectangle( { dx, dy, card_w, card_h }, 4.0f,
									bypass::overlay::color_t { 60, 10, 15, 200 }, 1.5f );

								constexpr float im = 4.0f;
								// Gold inner border
								bypass::overlay::rounded_rectangle( { dx + im, dy + im, card_w - 2.0f * im, card_h - 2.0f * im }, 2.0f,
									bypass::overlay::color_t { 210, 180, 100, 120 }, 1.0f );

								// Center emblem calculations (Fixed undefined variables)
								const float em_cx = dx + card_w * 0.5f;
								const float em_cy = dy + card_h * 0.5f;
								const float em_r = card_h * 0.18f;

								// Dice Icon (\xF522) - Gold
								bypass::overlay::text_ex( L"\xf521",
									{ em_cx - em_r, em_cy - em_r, em_r * 2.0f, em_r * 2.0f },
									bypass::overlay::color_t { 235, 205, 120, 220 }, em_r * 1.4f,
									bypass::overlay::text_weight_t::regular, L"Font Awesome",
									bypass::overlay::text_align_t::center, false );
							}
						}

						// Dealer hand — cards fly from deck to left-center
						for ( int i = 0; i < s_bj_dcount; ++i ) {
							const float final_cx = dealer_base_cx - ( s_bj_dcount - 1 ) * card_overlap * 0.5f + i * card_overlap - card_w * 0.5f;
							const bool  is_hole = ( i == 1 && hole_hidden );
							const float a = card_alpha( s_bj_dcard_at[ i ] );
							const float cx = deck_draw_cx + a * ( final_cx - deck_draw_cx );
							const float cy = deck_draw_cy + a * ( card_y - deck_draw_cy );
							bj_draw_card( cx, cy, card_w, card_h, s_bj_dcards[ i ], a, is_hole );
						}
						// Player hand — cards fly from deck to right-center
						for ( int i = 0; i < s_bj_pcount; ++i ) {
							const float final_cx = player_base_cx - ( s_bj_pcount - 1 ) * card_overlap * 0.5f + i * card_overlap - card_w * 0.5f;
							const float a = card_alpha( s_bj_pcard_at[ i ] );
							const float cx = deck_draw_cx + a * ( final_cx - deck_draw_cx );
							const float cy = deck_draw_cy + a * ( card_y - deck_draw_cy );
							bj_draw_card( cx, cy, card_w, card_h, s_bj_pcards[ i ], a, false );
						}

						// Dealer & Player Badges — widened chips, no clipping
						{
							const float badge_w = 140.0f;   // was 110 — extra room so label never hits the value
							const float badge_h = 40.0f;    // was 36 — a bit more presence
							const float badge_y = card_y + card_h + 14.0f;

							// ── Dealer Badge ───────────────────────────────────────
							const float dl_x = dealer_base_cx - badge_w * 0.5f;
							bypass::overlay::filled_rounded_rectangle( { dl_x, badge_y, badge_w, badge_h }, 20.0f,
								bypass::overlay::color_t { 22, 19, 35, 220 } );
							bypass::overlay::rounded_rectangle( { dl_x, badge_y, badge_w, badge_h }, 20.0f,
								bypass::overlay::color_t { 255, 255, 255, 12 }, 1.0f );

							// Red indicator dot
							bypass::overlay::filled_circle( { dl_x + 18.0f, badge_y + badge_h * 0.5f }, 4.5f,
								bypass::overlay::color_t { 239, 68, 68, 255 } );

							// Label — own dedicated zone (30..92), never touches the pill
							bypass::overlay::text_ex( L"DEALER",
								{ dl_x + 30.0f, badge_y, 62.0f, badge_h },
								bypass::overlay::color_t { 200, 196, 220, 160 }, 11.0f,
								bypass::overlay::text_weight_t::bold, L"Inter-Bold",
								bypass::overlay::text_align_t::left, false );

							// Value pill — its own zone on the right (93..133)
							wchar_t d_buf[ 16 ];
							std::swprintf( d_buf, 16, L"%d",
								hole_hidden ? bj_hand_value( s_bj_dcards, 1, nullptr ) : d_val );
							const float d_val_w = 40.0f;
							const float d_val_x = dl_x + badge_w - d_val_w - 7.0f;
							bypass::overlay::filled_rounded_rectangle(
								{ d_val_x, badge_y + 7.0f, d_val_w, badge_h - 14.0f }, 13.0f,
								bypass::overlay::color_t { 239, 68, 68, 35 } );
							bypass::overlay::text_ex( d_buf,
								{ d_val_x, badge_y + 7.0f, d_val_w, badge_h - 14.0f },
								bypass::overlay::color_t { 255, 255, 255, 240 }, 13.0f,
								bypass::overlay::text_weight_t::black, L"Geist-Bold",
								bypass::overlay::text_align_t::center, false );

							// ── Player Badge ───────────────────────────────────────
							const float pl_x = player_base_cx - badge_w * 0.5f;
							bypass::overlay::filled_rounded_rectangle( { pl_x, badge_y, badge_w, badge_h }, 20.0f,
								bypass::overlay::color_t { 22, 19, 35, 220 } );
							bypass::overlay::rounded_rectangle( { pl_x, badge_y, badge_w, badge_h }, 20.0f,
								bypass::overlay::color_t { 255, 255, 255, 12 }, 1.0f );

							// Green indicator dot
							bypass::overlay::filled_circle( { pl_x + 18.0f, badge_y + badge_h * 0.5f }, 4.5f,
								bypass::overlay::color_t { 61, 200, 122, 255 } );

							// Label
							bypass::overlay::text_ex( L"YOU",
								{ pl_x + 30.0f, badge_y, 62.0f, badge_h },
								bypass::overlay::color_t { 200, 196, 220, 160 }, 11.0f,
								bypass::overlay::text_weight_t::bold, L"Inter-Bold",
								bypass::overlay::text_align_t::left, false );

							// Value pill (wider when soft so "17 S" fits)
							wchar_t p_buf[ 16 ];
							std::swprintf( p_buf, 16, L"%d%s", p_val, p_soft ? L" S" : L"" );
							const float p_val_w = p_soft ? 46.0f : 40.0f;
							const float p_val_x = pl_x + badge_w - p_val_w - 7.0f;
							bypass::overlay::filled_rounded_rectangle(
								{ p_val_x, badge_y + 7.0f, p_val_w, badge_h - 14.0f }, 13.0f,
								bypass::overlay::color_t { 61, 200, 122, 35 } );
							bypass::overlay::text_ex( p_buf,
								{ p_val_x, badge_y + 7.0f, p_val_w, badge_h - 14.0f },
								bypass::overlay::color_t { 255, 255, 255, 240 }, 13.0f,
								bypass::overlay::text_weight_t::black, L"Geist-Bold",
								bypass::overlay::text_align_t::center, false );
						}
					}
				}
				// ══════════════════════════════════════════════════════════════════
				// CHICKEN
				// ══════════════════════════════════════════════════════════════════
				else if ( s_gamble_tab == 3 )
				{
					// Per-lane random speed and phase offsets (seeded when placing a bet)
					static float s_ck_lane_speed_mult[ 5 ] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
					static float s_ck_lane_phase_offset[ 5 ] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

					constexpr int   ck_max_lanes = 5;
					constexpr float hop_dur = 0.35f;
					constexpr float hit_dur = 0.7f;

					// Risk multipliers per lane
					const float risk_mults[ ] = { 1.15f, 1.35f, 1.6f }; // safe, spicy, wild
					const float mult = risk_mults[ s_chicken_risk ];

					// ── Road geometry ─────────────────────────────────────────────
					const float road_x = arena_x + 6.0f;
					const float road_w = arena_w - 12.0f;
					const float road_y = arena_y + 6.0f;
					const float road_h = arena_h - 12.0f;
					constexpr float curb_w = 16.0f;
					const float lanes_x = road_x + curb_w;
					const float lanes_w = road_w - curb_w * 2.0f;
					const float lane_w = lanes_w / ck_max_lanes;

					// ── Traffic definition ──
					struct lane_traffic_t { float speed; bool down; };
					const float risk_speed_scales[ ] = { 1.0f, 1.7f, 2.8f };
					const float risk_speed = risk_speed_scales[ s_chicken_risk ];

					const lane_traffic_t traffic[ ck_max_lanes ] = {
						{ 180.0f * risk_speed, true  }, { 140.0f * risk_speed, false },
						{ 200.0f * risk_speed, true  }, { 150.0f * risk_speed, false },
						{ 184.0f * risk_speed, true  }
					};
					constexpr float car_w = 20.0f, car_h = 30.0f;

					auto car_center_y = [ & ]( int lane, float t ) -> float {
						const float lane_speed_mult = 1.0f + static_cast< float >( lane ) * 0.5f;
						const float travel = road_h + car_h;
						// Apply random speed and phase offset
						const float speed = traffic[ lane ].speed * lane_speed_mult * s_ck_lane_speed_mult[ lane ];
						const float phase = std::fmod( t * speed + s_ck_lane_phase_offset[ lane ], travel );
						const float top = traffic[ lane ].down ? ( road_y - car_h + phase ) : ( road_y + road_h - phase );
						return top + car_h * 0.5f;
						};

					// ── Hop animation — 2D AABB collision check ──
									// ── Hop animation — 2D AABB collision check (fixed) ──
				// ── Hop animation — 2D AABB collision check ──
					float ck_visual_progress = s_ck_caught ? s_ck_hit_progress : static_cast< float >( s_ck_crossed );
					float hop_bounce = 0.0f;
					if ( s_ck_hopping && !s_ck_caught ) {
						const float ht_raw = ( animation_time - s_ck_hop_start ) / hop_dur;
						const int crossing_lane = s_ck_crossed;

						// Advance the chicken's visual position BEFORE the hit test so
						// the check uses where the bird actually is this frame,
						// not the lane edge it hopped from.
						if ( ht_raw < 1.0f )
							ck_visual_progress = static_cast< float >( crossing_lane ) + ht_raw;

						const float ck_cx = lanes_x + ck_visual_progress * lane_w;
						const float car_cx = lanes_x + crossing_lane * lane_w + lane_w * 0.5f;
						const float dx = std::abs( car_cx - ck_cx );

						// Car Y now and one frame ago — swept test so fast cars can't tunnel
						const float car_cy = car_center_y( crossing_lane, animation_time );
						const float car_cy_prev = car_center_y( crossing_lane, animation_time - gm_dt );

						// FIX: when the car loops back to the road start it teleports from one
						// edge to the other between frames. The swept segment would then span
						// the whole road and "cover" the chicken even though the car is only
						// half inside the interface. Detect the teleport and fall back to a
						// plain point test (a car at the edge can never be near arena_cy).
						const bool wrapped = std::abs( car_cy - car_cy_prev ) > car_h * 2.0f;

						const float hit_dx = car_w * 0.5f + 12.0f;
						const float hit_dy = car_h * 0.5f + 12.0f;

						bool y_hit;
						if ( wrapped )
							y_hit = std::abs( car_cy - arena_cy ) < hit_dy;
						else {
							const float y_lo = ( std::min ) ( car_cy_prev, car_cy ) - hit_dy;
							const float y_hi = ( std::max ) ( car_cy_prev, car_cy ) + hit_dy;
							y_hit = arena_cy >= y_lo && arena_cy <= y_hi;
						}

						if ( s_ck_server_caught && ht_raw >= 0.55f ) {
							s_ck_caught = true;
							s_ck_hit_at = animation_time;
							s_ck_hit_lane = crossing_lane;
							s_ck_frozen_car_y = car_cy;
							s_ck_hit_progress = ck_visual_progress;
							s_ck_hopping = false;
							s_game_active = false;
							s_game_result = -1;
							s_game_result_at = animation_time;
							s_result_tab = s_gamble_tab;
						}

						if ( !s_ck_caught ) {
							if ( ht_raw >= 1.0f ) {
								s_ck_hopping = false;
								s_ck_crossed = s_server_game.crossed;
								ck_visual_progress = static_cast< float >( s_ck_crossed );
								if ( s_ck_crossed >= ck_max_lanes ) {
									s_game_active = false;
									s_ck_result_mult = static_cast< float >( s_server_game.multiplier );
									s_game_result = 1;
									s_game_result_at = animation_time;
									s_result_tab = s_gamble_tab;
								}
							}
							else {
								hop_bounce = std::sin( ht_raw * 3.14159265f ) * 14.0f;
							}
						}
					}

					const float hit_t = s_ck_caught ? ( animation_time - s_ck_hit_at ) : 999.0f;
					const bool  in_hit = s_ck_caught && hit_t < hit_dur;

					// ── Asphalt ──────────────────────────────────────────────────
					bypass::overlay::filled_rounded_rectangle( { road_x, road_y, road_w, road_h }, 10.0f,
						bypass::overlay::color_t { 35, 34, 40, 255 } );

					// Curbs
					auto draw_curb = [ & ]( float cx ) {
						constexpr float dash_h = 14.0f;
						int idx = 0;
						for ( float y = road_y; y < road_y + road_h; y += dash_h, ++idx ) {
							bypass::overlay::filled_rectangle(
								{ cx, y, curb_w, ( std::min ) ( dash_h, road_y + road_h - y ) },
								( idx % 2 == 0 )
								? bypass::overlay::color_t { 60, 58, 68, 255 }
							: bypass::overlay::color_t { 42, 40, 48, 255 } );
						}
						};
					draw_curb( road_x );
					draw_curb( road_x + road_w - curb_w );

					// Lane dividers — dashed
					for ( int i = 1; i < ck_max_lanes; ++i ) {
						const float lx = lanes_x + i * lane_w;
						for ( float y = road_y + 4.0f; y < road_y + road_h - 4.0f; y += 18.0f ) {
							bypass::overlay::filled_rectangle( { lx - 1.0f, y, 2.0f, 10.0f },
								bypass::overlay::color_t { 255, 255, 255, 70 } );
						}
					}

					// Traffic
					{
						const bypass::overlay::color_t car_cols[ ck_max_lanes ] = {
							{  91, 146, 255, 255 }, { 239,  68,  68, 255 }, { 245, 196,  66, 255 },
							{  61, 200, 122, 255 }, { 170, 120, 255, 255 },
						};
						bypass::overlay::push_clip( { road_x, road_y, road_w, road_h } );
						for ( int i = 0; i < ck_max_lanes; ++i ) {
							const float lcx = lanes_x + i * lane_w + lane_w * 0.5f;
							const float car_cy_center = ( s_ck_caught && i == s_ck_hit_lane )
								? s_ck_frozen_car_y
								: car_center_y( i, animation_time );
							const float cy = car_cy_center - car_h * 0.5f;
							const auto& ccol = car_cols[ i ];
							bypass::overlay::filled_rounded_rectangle( { lcx - car_w * 0.5f, cy, car_w, car_h }, 5.0f, ccol );
							bypass::overlay::rounded_rectangle( { lcx - car_w * 0.5f, cy, car_w, car_h }, 5.0f,
								bypass::overlay::color_t { 0, 0, 0, 90 }, 1.0f );
							const float wind_y = traffic[ i ].down ? cy + car_h * 0.62f : cy + car_h * 0.14f;
							bypass::overlay::filled_rounded_rectangle(
								{ lcx - car_w * 0.32f, wind_y, car_w * 0.64f, car_h * 0.22f }, 2.0f,
								bypass::overlay::color_t { 20, 18, 26, 190 } );
							const float hl_y = traffic[ i ].down ? cy + car_h - 5.0f : cy + 2.0f;
							bypass::overlay::filled_rectangle( { lcx - car_w * 0.36f, hl_y, 3.0f, 3.0f },
								bypass::overlay::color_t { 255, 244, 200, 220 } );
							bypass::overlay::filled_rectangle( { lcx + car_w * 0.36f - 3.0f, hl_y, 3.0f, 3.0f },
								bypass::overlay::color_t { 255, 244, 200, 220 } );
						}
						bypass::overlay::pop_clip( );
					}

					// Multiplier signs per lane
					for ( int i = 0; i < ck_max_lanes; ++i ) {
						const float lx = lanes_x + i * lane_w;
						const bool  crossed_i = i < s_ck_crossed;
						wchar_t m_buf[ 16 ];
						std::swprintf( m_buf, 16, L"×%.2f", std::pow( mult, i + 1 ) );
						bypass::overlay::text_ex( m_buf,
							{ lx, road_y + 5.0f, lane_w, 14.0f },
							crossed_i ? bypass::overlay::color_t { 249, 115, 22, 255 } : bypass::overlay::color_t { 220, 216, 232, 150 },
							9.0f, bypass::overlay::text_weight_t::bold,
							L"Inter-Bold", bypass::overlay::text_align_t::center, false );
					}

					// ── Chicken ──────────────────────────────────────────────────
					{
						const float ck_hit_base_x = lanes_x + s_ck_hit_progress * lane_w;
						const float ck_cx = s_ck_caught
							? ck_hit_base_x + ( hit_t < hit_dur ? std::sin( ( hit_t / hit_dur ) * 3.14159265f ) * 20.0f : 20.0f )
							: lanes_x + ck_visual_progress * lane_w;
						float ck_cy = arena_cy - hop_bounce;
						if ( s_ck_caught && hit_t < hit_dur )
							ck_cy = arena_cy - std::sin( ( hit_t / hit_dur ) * 3.14159265f ) * 38.0f;
						const float shadow_r = 7.0f - hop_bounce * 0.15f;
						const std::uint8_t shadow_a = static_cast< std::uint8_t >( ( std::max ) ( 20.0f, 70.0f - hop_bounce * 3.5f ) );
						bypass::overlay::filled_circle( { ck_cx, arena_cy + 11.0f }, shadow_r,
							bypass::overlay::color_t { 0, 0, 0, shadow_a } );

						if ( s_ck_caught && hit_t < hit_dur * 4.0f ) {
							const float blood_f = 1.0f - ( hit_t / ( hit_dur * 4.0f ) );
							const std::uint8_t ba = static_cast< std::uint8_t >( blood_f * 210.0f );
							const float bx = ck_hit_base_x;
							const float by = s_ck_frozen_car_y;
							bypass::overlay::filled_circle( { bx,        by }, 5.5f, bypass::overlay::color_t { 180, 18, 18, ba } );
							bypass::overlay::filled_circle( { bx + 8.0f, by - 5.0f }, 3.2f, bypass::overlay::color_t { 200, 25, 25, ba } );
							bypass::overlay::filled_circle( { bx - 6.0f, by + 7.0f }, 2.8f, bypass::overlay::color_t { 155, 12, 12, ba } );
							bypass::overlay::filled_circle( { bx + 3.0f, by + 9.0f }, 2.0f, bypass::overlay::color_t { 190, 22, 22, ba } );
							bypass::overlay::filled_circle( { bx - 9.0f, by - 3.0f }, 2.5f, bypass::overlay::color_t { 170, 18, 18, ba } );
						}

						{
							const wchar_t* ck_icon = s_ck_caught ? L"\xF535" : ( s_ck_hopping ? L"\xF4BA" : L"\xF520" );
							const bypass::overlay::color_t ck_icon_col = s_ck_caught
								? bypass::overlay::color_t { 239, 68, 68, 255 }
							: bypass::overlay::color_t { 249, 115, 22, 255 };
							constexpr float ck_icon_sz = 24.0f;
							bypass::overlay::text_ex( ck_icon,
								{ ck_cx - ck_icon_sz * 0.5f, ck_cy - ck_icon_sz * 0.75f, ck_icon_sz, ck_icon_sz },
								ck_icon_col, ck_icon_sz,
								bypass::overlay::text_weight_t::bold, L"Font Awesome",
								bypass::overlay::text_align_t::center, false );
						}

						if ( in_hit ) {
							const float impact_t = ( std::min ) ( hit_t / hit_dur, 1.0f );
							const float ring_r = 6.0f + impact_t * 26.0f;
							const std::uint8_t ring_a = static_cast< std::uint8_t >( ( std::max ) ( 0.0f, 1.0f - impact_t ) * 200.0f );
							bypass::overlay::filled_circle( { ck_hit_base_x, s_ck_frozen_car_y }, ring_r,
								bypass::overlay::color_t { 255, 100, 100, ring_a } );

							constexpr int feather_count = 6;
							for ( int f = 0; f < feather_count; ++f ) {
								const float fang = ( static_cast< float >( f ) / feather_count ) * 3.14159265f * 2.0f + f * 0.7f;
								const float fdist = impact_t * 22.0f;
								const float fx = ck_hit_base_x + std::cos( fang ) * fdist;
								const float fy = s_ck_frozen_car_y + std::sin( fang ) * fdist * 0.6f + impact_t * impact_t * 18.0f;
								const std::uint8_t fa = static_cast< std::uint8_t >( ( std::max ) ( 0.0f, 1.0f - impact_t ) * 220.0f );
								bypass::overlay::filled_circle( { fx, fy }, 2.2f,
									bypass::overlay::color_t { 249, 115, 22, fa } );
							}
						}
					}
				}
				// ══════════════════════════════════════════════════════════════════
				// CRASH
				// ══════════════════════════════════════════════════════════════════
				else if ( s_gamble_tab == 4 )
				{
					const float elapsed = animation_time - s_game_start;
					constexpr float crash_dur = 4.0f;

					// Cashout multipliers
					const float cashout_mults[ ] = { 1.5f, 2.0f, 5.0f, 10.0f };
					const float target_mult = cashout_mults[ s_crash_cashout ];

					// Multiplier curve — statically goes up, crashes at random time
					if ( s_game_active ) {
						const float t = elapsed / crash_dur;
						// Fixed curve shape so it always sweeps up identically
						s_crash_mult = 1.0f + std::pow( t, 1.6f ) * 12.0f;

						if ( t >= s_crash_time_frac ) {
							s_crash_mult = 1.0f + std::pow( s_crash_time_frac, 1.6f ) * 12.0f;
							s_crash_exploded = true;
							s_game_active = false;
						s_game_result = s_server_game.won ? 1 : -1;
							s_game_result_at = animation_time;
							s_result_tab = s_gamble_tab;
						}
					}

					// ── Graph area ────────────────────────────────────────────────
					const float graph_x = arena_x + 44.0f;
					const float graph_y = arena_y + 14.0f;
					const float graph_w = arena_w - 44.0f - 50.0f;
					const float graph_h = arena_h - 14.0f - 22.0f;

					const float axis_max = 14.0f; // Increased to fit the static curve

					// Y-axis gridlines + scale labels
					{
						const float candidates[ ] = { 1.0f, 2.0f, 4.0f, 6.0f, 8.0f, 10.0f, 12.0f };
						for ( float cv : candidates ) {
							const float cy = graph_y + graph_h
								- ( ( cv - 1.0f ) / ( axis_max - 1.0f + 0.01f ) ) * graph_h;
							if ( cy < graph_y - 1.0f || cy > graph_y + graph_h + 1.5f ) continue;
							bypass::overlay::line( { graph_x, cy }, { graph_x + graph_w, cy },
								bypass::overlay::color_t { 255, 255, 255, 16 }, 2.0f );
							wchar_t tick_buf[ 8 ];
							std::swprintf( tick_buf, 8, L"%.0fx", cv );
							bypass::overlay::text_ex( tick_buf,
								{ arena_x + 6.0f, cy - 6.0f, graph_x - arena_x - 10.0f, 12.0f },
								bypass::overlay::color_t { 140, 134, 165, 200 }, 8.5f,
								bypass::overlay::text_weight_t::bold, L"Inter-Bold",
								bypass::overlay::text_align_t::right, false );
						}
					}

					// Axes
					bypass::overlay::line(
						{ graph_x, graph_y + graph_h }, { graph_x + graph_w, graph_y + graph_h },
						bypass::overlay::color_t { 255, 255, 255, 35 }, 2.0f );
					bypass::overlay::line(
						{ graph_x, graph_y }, { graph_x, graph_y + graph_h },
						bypass::overlay::color_t { 255, 255, 255, 35 }, 2.0f );

					// Multiplier line
									// Multiplier line
					// Multiplier line
					// Multiplier line
					const float progress = s_game_active
						? ( std::min ) ( ( animation_time - s_game_start ) / crash_dur, s_crash_time_frac )
						: ( s_crash_exploded ? s_crash_time_frac : 0.0f );

					bypass::overlay::point_t tip { graph_x, graph_y + graph_h };
					if ( progress > 0.0f ) {
						constexpr int segments = 60;
						const int draw_segs = static_cast< int >( progress * segments );
						bypass::overlay::point_t prev { graph_x, graph_y + graph_h };
						for ( int i = 1; i <= draw_segs; ++i ) {
							const float t = static_cast< float >( i ) / segments;
							// Static curve shape
							const float m = 1.0f + std::pow( t, 1.6f ) * 12.0f;
							const float ny = graph_y + graph_h - ( ( m - 1.0f ) / ( axis_max - 1.0f + 0.01f ) ) * graph_h;
							const float nx = graph_x + t * graph_w;

							const bool is_crash_color = s_crash_exploded && i == draw_segs;
							const auto line_col = is_crash_color ? bypass::overlay::color_t { 239, 68, 68, 255 }
							: bypass::overlay::color_t { 150, 90, 255, 255 };

							// 2. Inner Core (Solid, thicker than before)
							bypass::overlay::line( prev, { nx, ny }, line_col, 3.5f );

							prev = { nx, ny };
						}
						tip = prev;
					}

					// Target cashout line
					const float target_y = graph_y + graph_h
						- ( ( target_mult - 1.0f ) / ( axis_max - 1.0f + 0.01f ) ) * graph_h;
					if ( target_y > graph_y && target_y < graph_y + graph_h ) {
						// Target line glow
						bypass::overlay::line(
							{ graph_x, target_y }, { graph_x + graph_w, target_y },
							bypass::overlay::color_t { 245, 196, 66, 40 }, 5.0f );

						// Target line core
						bypass::overlay::line(
							{ graph_x, target_y }, { graph_x + graph_w, target_y },
							bypass::overlay::color_t { 245, 196, 66, 200 }, 2.5f );

						wchar_t tgt_buf[ 16 ];
						std::swprintf( tgt_buf, 16, L"%.1fx", target_mult );
						bypass::overlay::text_ex( tgt_buf,
							{ graph_x + graph_w + 8.0f, target_y - 8.0f, 44.0f, 16.0f },
							bypass::overlay::color_t { 245, 196, 66, 240 }, 11.0f,
							bypass::overlay::text_weight_t::black,
							L"Geist-Bold", bypass::overlay::text_align_t::left, false );
					}

					// Plane icon at the tip + Crash explosion effect
					if ( progress > 0.0f ) {
						// Matches the line color: Purple when flying, Red when crashed
						const auto plane_col = s_crash_exploded ? bypass::overlay::color_t { 239, 68, 68, 255 }
						: bypass::overlay::color_t { 150, 90, 255, 255 };

						if ( s_crash_exploded ) {

							// Explosion ring when crashed
							const float boom_age = animation_time - s_game_result_at;
							if ( boom_age < 0.6f ) {
								const float boom_t = boom_age / 0.6f;
								const float boom_r = 8.0f + boom_t * 35.0f;
								const std::uint8_t boom_a = static_cast< std::uint8_t >( ( 1.0f - boom_t ) * 220.0f );
								bypass::overlay::filled_circle( tip, boom_r,
									bypass::overlay::color_t { 239, 68, 68, boom_a } );
							}
						}

						wchar_t mult_buf[ 16 ];
						std::swprintf( mult_buf, 16, L"%.2fx", s_crash_mult );

						const float pill_w = 70.0f;
						const float pill_h = 42.0f;

						// Prevent pill from clipping off the left/right edges of the graph
						float pill_cx = tip.x;
						if ( pill_cx > graph_x + graph_w - pill_w * 0.5f - 5.0f ) pill_cx = graph_x + graph_w - pill_w * 0.5f - 5.0f;
						if ( pill_cx < graph_x + pill_w * 0.5f + 5.0f ) pill_cx = graph_x + pill_w * 0.5f + 5.0f;

						//// If too close to the bottom axis, draw above the plane. Otherwise below.
						float pill_cy = tip.y - 30.0f;
						//if ( pill_cy + pill_h * 0.5f > graph_y + graph_h - 5.0f ) {
						//	pill_cy = tip.y - 28.0f;
						//}

						const float pill_x = pill_cx - pill_w * 0.5f;
						const float pill_y = pill_cy - pill_h * 0.5f;

						// Pill Background
						bypass::overlay::filled_rounded_rectangle(
							{ pill_x, pill_y, pill_w, pill_h }, 8.0f,
							bypass::overlay::color_t { 20, 17, 31, 230 } );
						bypass::overlay::rounded_rectangle(
							{ pill_x, pill_y, pill_w, pill_h }, 8.0f,
							bypass::overlay::color_t { plane_col.r, plane_col.g, plane_col.b, 80 }, 1.0f );

						// Multiplier Text
						bypass::overlay::text_ex( mult_buf,
							{ pill_x, pill_y + 4.0f, pill_w, 20.0f },
							plane_col, 15.0f,
							bypass::overlay::text_weight_t::black, L"Geist-Bold",
							bypass::overlay::text_align_t::center, false );

						// Status Text
						const wchar_t* status_txt = s_crash_exploded ? L"CRASHED" : L"LIVE";
						const auto status_col = s_crash_exploded
							? bypass::overlay::color_t { 239, 68, 68, 220 }
						: bypass::overlay::color_t { 61, 200, 122, 220 };

						bypass::overlay::text_ex( status_txt,
							{ pill_x, pill_y + 24.0f, pill_w, 14.0f },
							status_col, 8.5f,
							bypass::overlay::text_weight_t::black, L"Inter-Bold",
							bypass::overlay::text_align_t::center, false );
					}

					//// ── Corner stat card ──────────────────────────────────────────
					//{
					//	constexpr float sc_w = 92.0f, sc_h = 46.0f;
					//	const float sc_x = arena_x + 12.0f;
					//	const float sc_y = arena_y + 12.0f;
					//	bypass::overlay::filled_rounded_rectangle( { sc_x, sc_y, sc_w, sc_h }, 8.0f,
					//		bypass::overlay::color_t { 20, 17, 31, 235 } );
					//	bypass::overlay::rounded_rectangle( { sc_x, sc_y, sc_w, sc_h }, 8.0f,
					//		bypass::overlay::color_t { 255, 255, 255, 26 }, 1.0f );

					//	wchar_t mult_buf[ 16 ];
					//	std::swprintf( mult_buf, 16, L"%.2fx", s_crash_mult );
					//	bypass::overlay::text_ex( mult_buf,
					//		{ sc_x + 4.0f, sc_y + 4.0f, sc_w - 8.0f, 22.0f },
					//		s_crash_exploded ? bypass::overlay::color_t { 239, 68, 68, 255 }
					//		: bypass::overlay::color_t { 170, 120, 255, 255 },
					//		16.0f, bypass::overlay::text_weight_t::black,
					//		L"Geist-Bold", bypass::overlay::text_align_t::center, false );

					//	const wchar_t* status_txt = s_crash_exploded ? L"CRASHED"
					//		: ( s_game_active ? L"LIVE" : L"READY" );
					//	const auto status_col = s_crash_exploded
					//		? bypass::overlay::color_t { 239, 68, 68, 210 }
					//		: ( s_game_active ? bypass::overlay::color_t { 61, 200, 122, 210 }
					//	: bypass::overlay::color_t { 140, 134, 165, 190 } );
					//	bypass::overlay::text_ex( status_txt,
					//		{ sc_x + 4.0f, sc_y + 27.0f, sc_w - 8.0f, 14.0f },
					//		status_col, 8.5f, bypass::overlay::text_weight_t::black,
					//		L"Inter-Bold", bypass::overlay::text_align_t::center, false );
					//}

					// ── Result modal ──────────────────────────────────────────────
	// ── Result modal — own zone, dead center, can't overlap anything ──
					if ( show_result ) {
						const float ov_a = ( std::max ) ( 0.0f, ( std::min ) ( 1.0f,
							result_age < 0.3f ? result_age / 0.3f
							: result_age > 2.0f ? ( 2.5f - result_age ) / 0.5f : 1.0f ) );
						const std::uint8_t ov_a8 = static_cast< std::uint8_t >( ov_a * 255 );
						const bool win = s_game_result == 1;

						// Premium sizing
						constexpr float pw = 280.0f, ph = 100.0f;

						// Scale animation (easeOutBack) for that satisfying "pop"
						const float scale_t = ( std::min ) ( result_age / 0.35f, 1.0f );
						constexpr float c1 = 1.70158f;
						constexpr float c3 = c1 + 1.0f;
						const float scale = scale_t < 1.0f
							? 1.0f + c3 * std::pow( scale_t - 1.0f, 3.0f ) + c1 * std::pow( scale_t - 1.0f, 2.0f )
							: 1.0f;

						const float cw = pw * scale;
						const float ch = ph * scale;
						const float cx = arena_cx - cw * 0.5f;
						const float cy = arena_cy - ch * 0.5f;

						if ( ov_a > 0.01f && scale > 0.1f ) {
							const auto pop_col = win ? bypass::overlay::color_t { 61, 200, 122, 255 } : bypass::overlay::color_t { 239, 68, 68, 255 };
							const auto glow_col = win ? bypass::overlay::color_t { 61, 200, 122, 40 } : bypass::overlay::color_t { 239, 68, 68, 40 };
							const wchar_t* pop_icon = win ? L"\uf091" : L"\uf1e2"; // Trophy or Bomb

							wchar_t title_buf[ 32 ];
							wchar_t sub_buf[ 32 ];
							if ( win ) {
								std::swprintf( title_buf, 32, L"CASHED OUT!" );
								std::swprintf( sub_buf, 32, L"Escaped at %.2fx", s_crash_mult );
							}
							else {
								std::swprintf( title_buf, 32, L"CRASHED!" );
								std::swprintf( sub_buf, 32, L"Destroyed at %.2fx", s_crash_mult );
							}

							// 1. Dimmed backdrop
							bypass::overlay::filled_rounded_rectangle( { arena_x, arena_y, arena_w, arena_h }, 12.0f,
								bypass::overlay::color_t { 6, 4, 12, static_cast< std::uint8_t >( ov_a * 140 ) } );

							//// 2. Glowing aura behind the card
							//const float glow_r = ( std::max ) ( cw, ch ) * 0.7f;
							//bypass::overlay::filled_circle( { arena_cx, arena_cy }, glow_r,
							//	bypass::overlay::color_t { glow_col.r, glow_col.g, glow_col.b, static_cast< std::uint8_t >( ov_a * 35 ) } );

							// 3. Main Card Base
							const float card_r = 14.0f * scale;
							bypass::overlay::filled_rounded_rectangle( { cx, cy, cw, ch }, card_r,
								bypass::overlay::color_t { 18, 15, 28, ov_a8 } );

							// 4. Thick glowing border
							bypass::overlay::rounded_rectangle( { cx, cy, cw, ch }, card_r,
								bypass::overlay::color_t { pop_col.r, pop_col.g, pop_col.b, static_cast< std::uint8_t >( ov_a * 180 ) }, 2.5f * scale );

							// 5. Glassmorphism top highlight
							bypass::overlay::push_clip( { cx, cy, cw, ch * 0.45f } );
							bypass::overlay::filled_rounded_rectangle( { cx, cy, cw, ch }, card_r,
								bypass::overlay::color_t { 255, 255, 255, static_cast< std::uint8_t >( ov_a * 12 ) } );
							bypass::overlay::pop_clip( );

							// 6. Icon Badge
							const float icon_cx = cx + 48.0f * scale;
							const float icon_cy = arena_cy;
							const float icon_r = 24.0f * scale;

							bypass::overlay::filled_circle( { icon_cx, icon_cy }, icon_r,
								bypass::overlay::color_t { pop_col.r, pop_col.g, pop_col.b, static_cast< std::uint8_t >( ov_a * 35 ) } );
							bypass::overlay::rounded_rectangle( { icon_cx - icon_r, icon_cy - icon_r, icon_r * 2.0f, icon_r * 2.0f }, icon_r,
								bypass::overlay::color_t { pop_col.r, pop_col.g, pop_col.b, static_cast< std::uint8_t >( ov_a * 150 ) }, 2.0f * scale );

							bypass::overlay::text_ex( pop_icon,
								{ icon_cx - icon_r, icon_cy - icon_r, icon_r * 2.0f, icon_r * 2.0f },
								bypass::overlay::color_t { pop_col.r, pop_col.g, pop_col.b, ov_a8 },
								24.0f * scale, bypass::overlay::text_weight_t::regular, L"Font Awesome",
								bypass::overlay::text_align_t::center, false );

							// 7. Text Section
							const float text_x = icon_cx + icon_r + 18.0f * scale;
							const float text_w = cx + cw - text_x - 16.0f * scale;

							bypass::overlay::text_ex( title_buf,
								{ text_x, cy + 26.0f * scale, text_w, 26.0f * scale },
								bypass::overlay::color_t { pop_col.r, pop_col.g, pop_col.b, ov_a8 },
								19.0f * scale, bypass::overlay::text_weight_t::black, L"Geist-Bold",
								bypass::overlay::text_align_t::left, false );

							bypass::overlay::text_ex( sub_buf,
								{ text_x, cy + 54.0f * scale, text_w, 20.0f * scale },
								bypass::overlay::color_t { 220, 216, 232, ov_a8 },
								12.5f * scale, bypass::overlay::text_weight_t::bold, L"Inter-Bold",
								bypass::overlay::text_align_t::left, false );
						}
					}
				}
				// ── Per-game result popout (uses s_result_tab to track the originating game) ──
				if ( show_result && s_result_tab != 4 ) {
					const float ov_a = ( std::max ) ( 0.0f, ( std::min ) ( 1.0f,
						result_age < 0.3f ? result_age / 0.3f
						: result_age > 2.0f ? ( 2.5f - result_age ) / 0.5f : 1.0f ) );
					const std::uint8_t ov_a8 = static_cast< std::uint8_t >( ov_a * 255 );
					const bool win = s_game_result == 1;
					const bool push = s_game_result == 2;

					// Premium sizing
					constexpr float pw = 280.0f, ph = 100.0f;

					// Scale animation (easeOutBack) for that satisfying "pop"
					const float scale_t = ( std::min ) ( result_age / 0.35f, 1.0f );
					constexpr float c1 = 1.70158f;
					constexpr float c3 = c1 + 1.0f;
					const float scale = scale_t < 1.0f
						? 1.0f + c3 * std::pow( scale_t - 1.0f, 3.0f ) + c1 * std::pow( scale_t - 1.0f, 2.0f )
						: 1.0f;

					const float cw = pw * scale;
					const float ch = ph * scale;
					const float cx = arena_cx - cw * 0.5f;
					const float cy = arena_cy - ch * 0.5f;

					if ( ov_a > 0.01f && scale > 0.1f ) {
						wchar_t pop_title[ 48 ] = {};
						wchar_t pop_sub[ 48 ] = {};
						const wchar_t* pop_icon = L"";
						bypass::overlay::color_t pop_col = { 61, 200, 122, 255 };
						bypass::overlay::color_t glow_col = { 61, 200, 122, 50 };

						// Dynamic, exciting text and icons based on the game
						if ( s_result_tab == 0 ) { // Mines
							if ( win ) {
								pop_col = { 245, 196, 66, 255 }; glow_col = { 245, 196, 66, 40 };
								pop_icon = L"\uf091"; // Trophy
								std::swprintf( pop_title, 48, L"CLEARED!" );
								std::swprintf( pop_sub, 48, L"Payout \x00D7%.2f", s_mn_mult );
							}
							else {
								pop_col = { 239, 68, 68, 255 }; glow_col = { 239, 68, 68, 40 };
								pop_icon = L"\uf1e2"; // Bomb
								std::swprintf( pop_title, 48, L"BOOM!" );
								std::swprintf( pop_sub, 48, L"Mine detonated" );
							}
						}
						else if ( s_result_tab == 1 ) { // Roulette
							if ( win ) {
								pop_col = { 61, 200, 122, 255 }; glow_col = { 61, 200, 122, 40 };
								pop_icon = L"\uf091"; // Trophy
								std::swprintf( pop_title, 48, L"WINNER!" );
								std::swprintf( pop_sub, 48, L"Payout \x00D7%.0f", s_rl_win_mult );
							}
							else {
								pop_col = { 239, 68, 68, 255 }; glow_col = { 239, 68, 68, 40 };
								pop_icon = L"\uf54c"; // Skull
								std::swprintf( pop_title, 48, L"BUSTED!" );
								std::swprintf( pop_sub, 48, L"Better luck next time" );
							}
						}
						else if ( s_result_tab == 2 ) { // Blackjack
							if ( push ) {
								pop_col = { 245, 196, 66, 255 }; glow_col = { 245, 196, 66, 40 };
								pop_icon = L"\uf06e"; // Eye / Neutral
								std::swprintf( pop_title, 48, L"PUSH!" );
								std::swprintf( pop_sub, 48, L"Stake returned" );
							}
							else if ( win ) {
								pop_col = { 61, 200, 122, 255 }; glow_col = { 61, 200, 122, 40 };
								pop_icon = L"\uf521"; // Crown
								std::swprintf( pop_title, 48, L"BLACKJACK!" );
								std::swprintf( pop_sub, 48, L"Beat the dealer" );
							}
							else {
								pop_col = { 239, 68, 68, 255 }; glow_col = { 239, 68, 68, 40 };
								pop_icon = L"\uf54c"; // Skull
								std::swprintf( pop_title, 48, L"BUSTED!" );
								std::swprintf( pop_sub, 48, L"Dealer wins" );
							}
						}
						else if ( s_result_tab == 3 ) { // Chicken
							if ( win ) {
								pop_col = { 249, 115, 22, 255 }; glow_col = { 249, 115, 22, 40 };
								pop_icon = L"\uf520"; // Bird
								std::swprintf( pop_title, 48, L"SURVIVED!" );
								std::swprintf( pop_sub, 48, L"Payout \x00D7%.2f", s_ck_result_mult );
							}
							else {
								pop_col = { 239, 68, 68, 255 }; glow_col = { 239, 68, 68, 40 };
								pop_icon = L"\uf54c"; // Skull
								std::swprintf( pop_title, 48, L"ROADKILL!" );
								std::swprintf( pop_sub, 48, L"Squashed at \x00D7%.2f", s_ck_result_mult );
							}
						}

						// 1. Dimmed backdrop
						bypass::overlay::filled_rounded_rectangle( { arena_x, arena_y, arena_w, arena_h }, 12.0f,
							bypass::overlay::color_t { 6, 4, 12, static_cast< std::uint8_t >( ov_a * 140 ) } );

						// 2. Glowing aura behind the card
						//const float glow_r = ( std::max ) ( cw, ch ) * 0.7f;
						//bypass::overlay::filled_circle( { arena_cx, arena_cy }, glow_r,
						//	bypass::overlay::color_t { glow_col.r, glow_col.g, glow_col.b, static_cast< std::uint8_t >( ov_a * 35 ) } );

						// 3. Main Card Base
						const float card_r = 14.0f * scale;
						bypass::overlay::filled_rounded_rectangle( { cx, cy, cw, ch }, card_r,
							bypass::overlay::color_t { 18, 15, 28, ov_a8 } );

						// 4. Thick glowing border
						bypass::overlay::rounded_rectangle( { cx, cy, cw, ch }, card_r,
							bypass::overlay::color_t { pop_col.r, pop_col.g, pop_col.b, static_cast< std::uint8_t >( ov_a * 180 ) }, 2.5f * scale );

						// 5. Glassmorphism top highlight
						bypass::overlay::push_clip( { cx, cy, cw, ch * 0.45f } );
						bypass::overlay::filled_rounded_rectangle( { cx, cy, cw, ch }, card_r,
							bypass::overlay::color_t { 255, 255, 255, static_cast< std::uint8_t >( ov_a * 12 ) } );
						bypass::overlay::pop_clip( );

						// 6. Icon Badge
						const float icon_cx = cx + 48.0f * scale;
						const float icon_cy = arena_cy;
						const float icon_r = 24.0f * scale;

						bypass::overlay::filled_circle( { icon_cx, icon_cy }, icon_r,
							bypass::overlay::color_t { pop_col.r, pop_col.g, pop_col.b, static_cast< std::uint8_t >( ov_a * 35 ) } );
						bypass::overlay::rounded_rectangle( { icon_cx - icon_r, icon_cy - icon_r, icon_r * 2.0f, icon_r * 2.0f }, icon_r,
							bypass::overlay::color_t { pop_col.r, pop_col.g, pop_col.b, static_cast< std::uint8_t >( ov_a * 150 ) }, 2.0f * scale );

						bypass::overlay::text_ex( pop_icon,
							{ icon_cx - icon_r, icon_cy - icon_r, icon_r * 2.0f, icon_r * 2.0f },
							bypass::overlay::color_t { pop_col.r, pop_col.g, pop_col.b, ov_a8 },
							24.0f * scale, bypass::overlay::text_weight_t::regular, L"Font Awesome",
							bypass::overlay::text_align_t::center, false );

						// 7. Text Section
						const float text_x = icon_cx + icon_r + 18.0f * scale;
						const float text_w = cx + cw - text_x - 16.0f * scale;

						bypass::overlay::text_ex( pop_title,
							{ text_x, cy + 26.0f * scale, text_w, 26.0f * scale },
							bypass::overlay::color_t { pop_col.r, pop_col.g, pop_col.b, ov_a8 },
							19.0f * scale, bypass::overlay::text_weight_t::black, L"Geist-Bold",
							bypass::overlay::text_align_t::left, false );

						bypass::overlay::text_ex( pop_sub,
							{ text_x, cy + 54.0f * scale, text_w, 20.0f * scale },
							bypass::overlay::color_t { 220, 216, 232, ov_a8 },
							12.5f * scale, bypass::overlay::text_weight_t::bold, L"Inter-Bold",
							bypass::overlay::text_align_t::left, false );
					}

				}

				// ══════════════════════════════════════════════════════════════════
				// BETTING BAR
				// ══════════════════════════════════════════════════════════════════
				static std::uint32_t s_gamble_bet = 10;

				const float bar_x = page_x;
				const float bar_w = page_w;

				bypass::overlay::filled_rounded_rectangle(
					{ bar_x, bar_y, bar_w, gm_bar_h }, 12.0f,
					bypass::overlay::color_t { 22, 19, 35, 220 } );
				bypass::overlay::rounded_rectangle(
					{ bar_x, bar_y, bar_w, gm_bar_h }, 12.0f,
					bypass::overlay::color_t { 255, 255, 255, 22 }, 1.0f );

				// 1. Game Name (Left)
				const float game_lbl_x = bar_x + 18.0f;
				const float game_name_w = static_cast< float >( std::wcslen( active_game.name ) ) * 8.5f;
				bypass::overlay::text_ex( active_game.icon,
					{ game_lbl_x, bar_y, 24.0f, gm_bar_h },
					active_game.col, 14.0f, bypass::overlay::text_weight_t::regular,
					L"Font Awesome", bypass::overlay::text_align_t::center, false );
				bypass::overlay::text_ex( active_game.name,
					{ game_lbl_x + 26.0f, bar_y, game_name_w, gm_bar_h },
					bypass::overlay::color_t { 244, 241, 250, 230 }, 14.0f,
					bypass::overlay::text_weight_t::black,
					L"Geist-Bold", bypass::overlay::text_align_t::left, false );

				// 2. Balance (coin icon + value)
				const float bal_icon_x = game_lbl_x + 26.0f + game_name_w + 12.0f;
				const float bal_val_x = bal_icon_x + 20.0f;
				bypass::overlay::text_ex( L"\uf51e",
					{ bal_icon_x, bar_y, 16.0f, gm_bar_h },
					bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 220 }, 12.0f,
					bypass::overlay::text_weight_t::regular,
					L"Font Awesome", bypass::overlay::text_align_t::center, false );
				// Balance readout — "BROKE" fallback keeps the coin
				// bar consistent with the profile-page currency cards
				// so a 0-coin state reads the same everywhere.
				wchar_t bal_txt[ 24 ];
				if ( s_shop_coins == 0 )
					std::swprintf( bal_txt, std::size( bal_txt ), L"BROKE" );
				else
					std::swprintf( bal_txt, std::size( bal_txt ), L"%u", s_shop_coins );
				const float bal_txt_w = static_cast< float >( std::wcslen( bal_txt ) ) * 8.0f;
				bypass::overlay::text_ex( bal_txt,
					{ bal_val_x, bar_y, bal_txt_w + 8.0f, gm_bar_h },
					bypass::overlay::color_t { 220, 215, 230, 220 }, 13.0f,
					bypass::overlay::text_weight_t::black,
					L"Geist-Bold", bypass::overlay::text_align_t::left, false );

				// 3. Leftmost separator — 22px after the balance value
				const float left_sep_x = bal_val_x + bal_txt_w + 22.0f;
				bypass::overlay::filled_rectangle(
					{ left_sep_x, bar_y + 12.0f, 1.0f, gm_bar_h - 24.0f },
					bypass::overlay::color_t { 255, 255, 255, 22 } );

				// ── Right side: PLACE BET + bet controls ─────────────────────────
				constexpr float play_btn_w = 110.0f;
				constexpr float play_btn_h = 32.0f;
				const float play_btn_x = bar_x + bar_w - 14.0f - play_btn_w;
				const float play_btn_y = bar_y + ( gm_bar_h - play_btn_h ) * 0.5f;
				const bool play_hov = hit_test( mx, my, play_btn_x, play_btn_y, play_btn_w, play_btn_h );
				const bool can_retry = crisis::casino::client.configured( ) && !crisis::casino::client.ready( ) && !s_game_active;
				const bool can_bet = crisis::casino::client.ready( ) && s_shop_coins >= s_gamble_bet && s_gamble_bet > 0 && !s_game_active;
				const bool play_enabled = can_retry || can_bet;

				// Bet controls group — [1/2] [2x] [-] amount [+]
				constexpr float ctl_h = 28.0f;
				constexpr float plus_sz = 26.0f;
				constexpr float minus_sz = 26.0f;
				constexpr float amount_w = 58.0f;
				constexpr float inner_gap = 1.0f;
				constexpr float mid_gap = 14.0f;
				constexpr float dbl_w = 40.0f;
				constexpr float half_w = 40.0f;
				constexpr float small_gap = 6.0f;
				const float ctl_y = bar_y + ( gm_bar_h - ctl_h ) * 0.5f;

				const float group_right = play_btn_x - 14.0f;
				const float plus_x = group_right - plus_sz;
				const float amount_x = plus_x - inner_gap - amount_w;
				const float minus_x = amount_x - inner_gap - minus_sz;
				const float dbl_x = minus_x - mid_gap - dbl_w;
				const float half_x = dbl_x - small_gap - half_w;

				// Separator before the bet controls group
				const float right_sep_x = half_x - 14.0f;
				bypass::overlay::filled_rectangle(
					{ right_sep_x, bar_y + 12.0f, 1.0f, gm_bar_h - 24.0f },
					bypass::overlay::color_t { 255, 255, 255, 22 } );

				// ── Middle zone: per-game controls ───────────────────────────────
				const float mid_x = left_sep_x + 14.0f;
				const float mid_w = ( right_sep_x - 14.0f ) - mid_x;
				const float mid_y = ctl_y;
				const float mid_h = ctl_h;

				auto draw_seg = [ & ]( float sx, float sy, float sw, float sh,
					const wchar_t* const* labels, int count,
					std::uint32_t* selected,
					const bypass::overlay::color_t* opt_cols = nullptr ) {
						constexpr float seg_gap = 6.0f;
						const float seg_w = ( sw - seg_gap * ( count - 1 ) ) / static_cast< float >( count );
						for ( int i = 0; i < count; ++i ) {
							const float bx = sx + static_cast< float >( i ) * ( seg_w + seg_gap );
							const bool hov = hit_test( mx, my, bx, sy, seg_w, sh );
							const bool act = *selected == static_cast< std::uint32_t >( i );
							const auto accent = opt_cols ? opt_cols[ i ] : active_game.col;
							if ( clicked && hov )
								*selected = static_cast< std::uint32_t >( i );

							if ( act && opt_cols ) {
								bypass::overlay::filled_rounded_rectangle(
									{ bx, sy, seg_w, sh }, 7.0f,
									bypass::overlay::color_t { accent.r, accent.g, accent.b,
										static_cast< std::uint8_t >( 225 ) } );
								bypass::overlay::rounded_rectangle(
									{ bx, sy, seg_w, sh }, 7.0f,
									bypass::overlay::color_t { 255, 255, 255, 55 }, 1.0f );
								bypass::overlay::text_ex( labels[ i ],
									{ bx, sy, seg_w, sh },
									bypass::overlay::color_t { 255, 255, 255, 245 }, 11.0f,
									bypass::overlay::text_weight_t::black,
									L"Geist-Bold", bypass::overlay::text_align_t::center, false );
							}
							else {
								bypass::overlay::filled_rounded_rectangle(
									{ bx, sy, seg_w, sh }, 7.0f,
									act ? bypass::overlay::color_t { accent.r, accent.g, accent.b,
																	static_cast< std::uint8_t >( 42 ) }
									: bypass::overlay::color_t { 40, 38, 55,
																static_cast< std::uint8_t >( hov ? 255 : 200 ) } );
								bypass::overlay::rounded_rectangle(
									{ bx, sy, seg_w, sh }, 7.0f,
									act ? bypass::overlay::color_t { accent.r, accent.g, accent.b,
																	static_cast< std::uint8_t >( 150 ) }
								: bypass::overlay::color_t { 255, 255, 255, 22 }, 1.0f );
								bypass::overlay::text_ex( labels[ i ],
									{ bx, sy, seg_w, sh },
									act ? bypass::overlay::color_t { 255, 255, 255, 240 }
									: bypass::overlay::color_t { 220, 215, 230,
																static_cast< std::uint8_t >( hov ? 220 : 160 ) },
									11.0f, bypass::overlay::text_weight_t::bold,
									L"Inter-Bold", bypass::overlay::text_align_t::center, false );
							}
						}
					};

				if ( s_gamble_tab == 0 ) {
					if ( s_game_active ) {
						wchar_t co_buf[ 32 ];
						std::swprintf( co_buf, 32, L"Cash Out  \u00d7%.2f", s_mn_mult );
						const bool co_hov = hit_test( mx, my, mid_x, mid_y, mid_w, mid_h );
						bypass::overlay::filled_rounded_rectangle( { mid_x, mid_y, mid_w, mid_h }, 6.0f,
							co_hov ? bypass::overlay::color_t { 80, 200, 120, 255 } : bypass::overlay::color_t { 50, 160, 90, 255 } );
						bypass::overlay::text_ex( co_buf,
							{ mid_x, mid_y, mid_w, mid_h },
							bypass::overlay::color_t { 255, 255, 255, 255 }, 12.0f,
							bypass::overlay::text_weight_t::black,
							L"Geist-Bold", bypass::overlay::text_align_t::center, false );
						if ( clicked && co_hov && s_mn_revealed_count > 0 ) {
							s_server_game = crisis::casino::client.mines_cashout( );
							if ( !apply_casino_result( s_server_game ) ) return;
							std::memset( s_mn_mines, 0, sizeof( s_mn_mines ) );
							for ( const int mine : s_server_game.mine_positions ) if ( mine >= 0 && mine < 25 ) s_mn_mines[ mine ] = true;
							s_mn_cashed_out = true;
							s_game_active = false;
							s_game_result = 1;
							s_game_result_at = animation_time;
							s_result_tab = s_gamble_tab;

							s_mn_mult_pop_at = animation_time;

							// Cash-out reveal — same staggered timing as the loss reveal.
							{
								int _o = 0; for ( int _bj = 0; _bj < 25; ++_bj ) {
									if ( !s_mn_mines[ _bj ] )
										continue;

									s_mn_revealed[ _bj ] = true;
									s_mn_reveal_at[ _bj ] = animation_time + 0.12f + _o++ * 0.05f;
								}
							}
						}
					}
					else {
						static constexpr const wchar_t* opts[ ] = { L"Safe (3)", L"Spicy (5)", L"Wild (8)" };
						draw_seg( mid_x, mid_y, mid_w, mid_h, opts, 3, &s_coinflip_side );
					}
				}
				else if ( s_gamble_tab == 1 ) {
					static constexpr const wchar_t* opts[ ] = { L"Red ×2", L"Black ×2", L"Green ×36" };
					static constexpr bypass::overlay::color_t cols[ ] = {
						{ 239,  68,  68, 255 },
						{  72,  72,  80, 255 },
						{  34, 197,  94, 255 },
					};
					draw_seg( mid_x, mid_y, mid_w, mid_h, opts, 3, &s_roulette_pick, cols );
				}
				else if ( s_gamble_tab == 2 ) {
					if ( s_bj_phase == 1 ) {
						// Hit / Stand in bet bar during player turn
						const float hw = ( mid_w - 8.0f ) * 0.5f;
						const float sx = mid_x + hw + 8.0f;
						const bool  bj_hit_hov = hit_test( mx, my, mid_x, mid_y, hw, mid_h );
						const bool  bj_stand_hov = hit_test( mx, my, sx, mid_y, hw, mid_h );

						bypass::overlay::filled_rounded_rectangle( { mid_x, mid_y, hw, mid_h }, 8.0f,
							bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b,
								static_cast< std::uint8_t >( bj_hit_hov ? 255 : 215 ) } );
						// HIT probability: ranks that don't bust (13 total rank types)
						int bj_pv = 0, bj_ac = 0;
						for ( int _bi = 0; _bi < s_bj_pcount; ++_bi ) {
							int rv = s_bj_pcards[ _bi ].rank;
							if ( rv == 1 ) { ++bj_ac; rv = 11; }
							else if ( rv >= 10 ) rv = 10;
							bj_pv += rv;
						}
						while ( bj_pv > 21 && bj_ac > 0 ) { bj_pv -= 10; --bj_ac; }
						const int bj_need = 21 - bj_pv;
						const int bj_safe = bj_need >= 10 ? 13 : ( bj_need >= 1 ? bj_need : 0 );
						const int bj_pct_hit = bj_safe * 100 / 13;
						const int bj_pct_stand = 100 - bj_pct_hit;
						wchar_t bj_hit_buf[ 16 ], bj_stand_buf[ 16 ];
						std::swprintf( bj_hit_buf, 16, L"HIT (%d%%)", bj_pct_hit );
						std::swprintf( bj_stand_buf, 16, L"STAND (%d%%)", bj_pct_stand );
						bypass::overlay::text_ex( bj_hit_buf, { mid_x, mid_y, hw, mid_h },
							bypass::overlay::color_t { 255, 255, 255, 255 }, 12.0f,
							bypass::overlay::text_weight_t::black, L"Geist-Bold",
							bypass::overlay::text_align_t::center, false );

						bypass::overlay::filled_rounded_rectangle( { sx, mid_y, hw, mid_h }, 8.0f,
							bypass::overlay::color_t { 40, 38, 55, static_cast< std::uint8_t >( bj_stand_hov ? 255 : 210 ) } );
						bypass::overlay::rounded_rectangle( { sx, mid_y, hw, mid_h }, 8.0f,
							bypass::overlay::color_t { 255, 255, 255, 40 }, 1.0f );
						bypass::overlay::text_ex( bj_stand_buf, { sx, mid_y, hw, mid_h },
							bypass::overlay::color_t { 230, 226, 240, 255 }, 12.0f,
							bypass::overlay::text_weight_t::black, L"Geist-Bold",
							bypass::overlay::text_align_t::center, false );

						if ( clicked && bj_hit_hov ) {
							const int previous_count = s_bj_pcount;
							s_server_game = crisis::casino::client.blackjack_action( "hit" );
							if ( apply_casino_result( s_server_game ) ) {
								load_server_cards( s_server_game );
								for ( int i = previous_count; i < s_bj_pcount; ++i ) s_bj_pcard_at[ i ] = animation_time;
								if ( s_server_game.settled( ) ) { s_bj_phase = 2; s_bj_step_at = animation_time + 0.9f; }
							}
						}
						if ( clicked && bj_stand_hov ) {
							s_server_game = crisis::casino::client.blackjack_action( "stand" );
							if ( apply_casino_result( s_server_game ) ) { load_server_cards( s_server_game ); for ( int i = 0; i < s_bj_dcount; ++i ) s_bj_dcard_at[ i ] = animation_time + i * 0.2f; s_bj_phase = 2; s_bj_step_at = animation_time + 0.9f; }
						}
					}
					else {
						const wchar_t* bj_status = ( s_bj_phase == 0 ) ? L"Dealing…"
							: ( s_bj_phase == 2 ) ? L"Dealer's turn…"
							: L"Place a bet to play";
						bypass::overlay::text_ex( bj_status, { mid_x, mid_y, mid_w, mid_h },
							bypass::overlay::color_t { 200, 196, 220, 180 }, 10.5f,
							bypass::overlay::text_weight_t::bold, L"Geist-Bold",
							bypass::overlay::text_align_t::center, false );
					}
				}
				else if ( s_gamble_tab == 3 ) {
					static constexpr float ck_risk_mults[ 3 ] = { 1.15f, 1.5f, 2.75f };
					const float ck_mult = ck_risk_mults[ s_chicken_risk ];
					const bool ck_active = s_game_active && !s_ck_caught && s_ck_crossed < 5;
					if ( ck_active ) {
						const float jump_w = ( s_ck_crossed > 0 ) ? mid_w * 0.5f - 2.0f : mid_w;
						const bool  jump_hov = hit_test( mx, my, mid_x, mid_y, jump_w, mid_h );
						bypass::overlay::filled_rounded_rectangle( { mid_x, mid_y, jump_w, mid_h }, 7.0f,
							s_ck_hopping ? bypass::overlay::color_t { 60, 56, 46, 210 }
						: bypass::overlay::color_t { 249, 115, 22, static_cast< std::uint8_t >( jump_hov ? 255 : 228 ) } );
						bypass::overlay::rounded_rectangle( { mid_x, mid_y, jump_w, mid_h }, 7.0f,
							bypass::overlay::color_t { 255, 255, 255, 40 }, 1.0f );
						bypass::overlay::text_ex( s_ck_hopping ? L"…" : L"JUMP", { mid_x, mid_y, jump_w, mid_h },
							bypass::overlay::color_t { 255, 255, 255, static_cast< std::uint8_t >( s_ck_hopping ? 140u : 255u ) }, 12.0f,
							bypass::overlay::text_weight_t::black, L"Geist-Bold", bypass::overlay::text_align_t::center, false );
						if ( clicked && jump_hov && !s_ck_hopping ) {
							s_server_game = crisis::casino::client.chicken_jump( );
							if ( apply_casino_result( s_server_game ) ) { s_ck_server_caught = s_server_game.caught; s_ck_result_mult = static_cast< float >( s_server_game.multiplier ); s_ck_hopping = true; s_ck_hop_start = animation_time; }
						}
						if ( s_ck_crossed > 0 ) {
							const float co_x = mid_x + mid_w * 0.5f + 2.0f;
							const float co_w = mid_w * 0.5f - 2.0f;
							const bool  co_hov = hit_test( mx, my, co_x, mid_y, co_w, mid_h );
							bypass::overlay::filled_rounded_rectangle( { co_x, mid_y, co_w, mid_h }, 7.0f,
								bypass::overlay::color_t { 50, 160, 90, static_cast< std::uint8_t >( co_hov ? 255 : 220 ) } );
							bypass::overlay::rounded_rectangle( { co_x, mid_y, co_w, mid_h }, 7.0f,
								bypass::overlay::color_t { 255, 255, 255, 40 }, 1.0f );
							bypass::overlay::text_ex( L"CASH OUT", { co_x, mid_y, co_w, mid_h },
								bypass::overlay::color_t { 255, 255, 255, 255 }, 11.0f,
								bypass::overlay::text_weight_t::black, L"Geist-Bold", bypass::overlay::text_align_t::center, false );
							if ( clicked && co_hov ) {
								s_server_game = crisis::casino::client.chicken_cashout( );
								if ( apply_casino_result( s_server_game ) ) {
									// Client-side authoritative multiplier for the result popup.
									// The server's cashout multiplier is returning a
									// runaway value on early cashouts — pow-of-lanes based
									// but with the wrong base — so the "SURVIVED! Payout
									// ×N" line ends up nonsensical (30x, 100x etc). The
									// user-facing tier labels above ("Safe ×1.15", "Spicy
									// ×1.5", "Wild ×2.75") ARE the per-lane multipliers,
									// and the payout should compound them across lanes
									// actually crossed. Deriving it here from the
									// visible-to-user constants keeps the popup honest
									// against what the player picked without a server
									// round-trip fix.
									float derived_mult = 1.0f;
									for ( int lane_i = 0; lane_i < s_ck_crossed; ++lane_i )
										derived_mult *= ck_risk_mults[ s_chicken_risk ];
									s_ck_result_mult  = derived_mult;
									s_game_active     = false;
									s_game_result     = 1;
									s_game_result_at  = animation_time;
									s_result_tab      = s_gamble_tab;
								}
							}
						}
					}
					else {
						static constexpr const wchar_t* opts[ ] = { L"Safe ×1.15", L"Spicy ×1.5", L"Wild ×2.75" };
						draw_seg( mid_x, mid_y, mid_w, mid_h, opts, 3, &s_chicken_risk );
					}
				}
				else if ( s_gamble_tab == 4 ) {
					static constexpr const wchar_t* opts[ ] = { L"1.5x", L"2x", L"5x", L"10x" };
					draw_seg( mid_x, mid_y, mid_w, mid_h, opts, 4, &s_crash_cashout );
				}

				// ── Bet control buttons (1/2, 2x, -, +) ─────────────────────────
				bool half_hov = false, dbl_hov = false, minus_hov = false, plus_hov = false;
				auto draw_ctl_btn = [ & ]( float bx, float bw, const wchar_t* label,
					const wchar_t* font, float font_sz, bool& hov ) {
						hov = hit_test( mx, my, bx, ctl_y, bw, ctl_h );
						bypass::overlay::filled_rounded_rectangle(
							{ bx, ctl_y, bw, ctl_h }, 7.0f,
							bypass::overlay::color_t { 40, 38, 55,
								static_cast< std::uint8_t >( hov ? 255 : 200 ) } );
						bypass::overlay::rounded_rectangle(
							{ bx, ctl_y, bw, ctl_h }, 7.0f,
							bypass::overlay::color_t { 255, 255, 255, 22 }, 1.0f );
						bypass::overlay::text_ex( label,
							{ bx, ctl_y, bw, ctl_h },
							bypass::overlay::color_t { 220, 215, 230, 220 }, font_sz,
							bypass::overlay::text_weight_t::bold,
							font, bypass::overlay::text_align_t::center, false );
					};

				draw_ctl_btn( half_x, half_w, L"1/2", L"Inter-Bold", 11.0f, half_hov );
				draw_ctl_btn( dbl_x, dbl_w, L"2x", L"Inter-Bold", 11.0f, dbl_hov );
				draw_ctl_btn( minus_x, minus_sz, L"\uf068", L"Font Awesome", 10.0f, minus_hov );
				draw_ctl_btn( plus_x, plus_sz, L"\uf067", L"Font Awesome", 10.0f, plus_hov );

				// Bet amount text
				wchar_t bet_txt[ 16 ];
				std::swprintf( bet_txt, std::size( bet_txt ), L"%u", s_gamble_bet );
				bypass::overlay::text_ex( bet_txt,
					{ amount_x, ctl_y, amount_w, ctl_h },
					bypass::overlay::color_t { 255, 255, 255, 255 }, 15.0f,
					bypass::overlay::text_weight_t::black,
					L"Geist-Bold", bypass::overlay::text_align_t::center, false );

				// Bet control input
				if ( clicked && minus_hov && s_gamble_bet >= 5 ) s_gamble_bet -= 5;
				if ( clicked && plus_hov )                       s_gamble_bet += 5;
				if ( clicked && half_hov )                       s_gamble_bet = ( std::max ) ( 1u, s_gamble_bet / 2u );
				if ( clicked && dbl_hov )                        s_gamble_bet *= 2u;

				// ── PLACE BET draw ───────────────────────────────────────────────
				bypass::overlay::filled_rounded_rectangle(
					{ play_btn_x, play_btn_y, play_btn_w, play_btn_h }, play_btn_h * 0.5f,
					play_enabled ? bypass::overlay::color_t { active_game.col.r, active_game.col.g,
														active_game.col.b,
														static_cast< std::uint8_t >( play_hov ? 255 : 210 ) }
				: bypass::overlay::color_t { 60, 55, 75, 180 } );

				if ( play_enabled ) {
					bypass::overlay::push_clip( { play_btn_x, play_btn_y, play_btn_w, play_btn_h * 0.5f } );
					bypass::overlay::filled_rounded_rectangle(
						{ play_btn_x, play_btn_y, play_btn_w, play_btn_h }, play_btn_h * 0.5f,
						bypass::overlay::color_t { 255, 255, 255,
							static_cast< std::uint8_t >( play_hov ? 35 : 20 ) } );
					bypass::overlay::pop_clip( );
				}

				bypass::overlay::text_ex( can_retry ? L"RETRY" : L"PLACE BET",
					{ play_btn_x, play_btn_y, play_btn_w, play_btn_h },
					play_enabled ? bypass::overlay::color_t { 255, 255, 255, 220 }
					: bypass::overlay::color_t { 150, 145, 165, 150 },
					12.0f, bypass::overlay::text_weight_t::bold,
					L"Geist-Bold", bypass::overlay::text_align_t::center, false );

				// ── PLACE BET — trigger game ─────────────────────────────────────
				if ( clicked && play_hov && can_retry ) {
					apply_casino_result( crisis::casino::client.state( ) );
				}
				else if ( clicked && play_hov && can_bet ) {
					static constexpr int mine_counts[ 3 ] = { 3, 5, 8 };
					static constexpr const char* roulette_choices[ 3 ] = { "red", "black", "green" };
					static constexpr const char* chicken_risks[ 3 ] = { "safe", "spicy", "wild" };
					static constexpr double crash_targets[ 4 ] = { 1.5, 2.0, 5.0, 10.0 };
					switch ( s_gamble_tab ) {
					case 0: s_server_game = crisis::casino::client.mines_start( s_gamble_bet, mine_counts[ s_coinflip_side ] ); break;
					case 1: s_server_game = crisis::casino::client.roulette( s_gamble_bet, roulette_choices[ s_roulette_pick ] ); break;
					case 2: s_server_game = crisis::casino::client.blackjack_start( s_gamble_bet ); break;
					case 3: s_server_game = crisis::casino::client.chicken_start( s_gamble_bet, chicken_risks[ s_chicken_risk ] ); break;
					case 4: s_server_game = crisis::casino::client.crash( s_gamble_bet, crash_targets[ s_crash_cashout ] ); break;
					default: break;
					}
					if ( apply_casino_result( s_server_game ) ) {
						s_game_active = true;
						s_game_start = animation_time;
						s_game_bet = static_cast< float >( s_gamble_bet );
						s_game_result = 0;
						if ( s_gamble_tab == 0 ) {
						std::memset( s_mn_mines, 0, sizeof( s_mn_mines ) );
						std::memset( s_mn_revealed, 0, sizeof( s_mn_revealed ) );
						s_mn_revealed_count = 0;
						s_mn_mult = 1.0f;
						s_mn_cashed_out = false;
						for ( int _r = 0; _r < 25; ++_r ) s_mn_reveal_at[ _r ] = -100.0f;
						s_mn_mult_pop_at = -10.0f; s_mn_explode_at = -10.0f; s_mn_shake_at = -10.0f;
						}
						else if ( s_gamble_tab == 1 ) {
						s_rl_start = s_rl_pos;
						const float center_offset = ( arena_w - rl_item_w ) * 0.5f;
						const int cur_ti = static_cast< int >( std::round( ( s_rl_pos + center_offset ) / rl_per ) );
						const int desired = ( std::clamp )( s_server_game.number, 0, rl_seq_len - 1 );
						const int base = cur_ti + 5 * rl_seq_len;
						const int target = base + ( desired - ( ( base % rl_seq_len ) + rl_seq_len ) % rl_seq_len + rl_seq_len ) % rl_seq_len;
						s_rl_target = static_cast< float >( target ) * rl_per - center_offset;
						s_rl_result = ( target % rl_seq_len + rl_seq_len ) % rl_seq_len;
						s_rl_spin = true;
						s_rl_spin_start = animation_time;
					}
						else if ( s_gamble_tab == 2 ) {
						constexpr float deal_stagger = 0.4f;
						load_server_cards( s_server_game );
						for ( int i = 0; i < s_bj_pcount; ++i ) s_bj_pcard_at[ i ] = animation_time + static_cast< float >( i * 2 ) * deal_stagger;
						for ( int i = 0; i < s_bj_dcount; ++i ) s_bj_dcard_at[ i ] = animation_time + static_cast< float >( i * 2 + 1 ) * deal_stagger;
						s_bj_phase = s_server_game.status == "playing" ? 0 : 3;
						if ( s_server_game.settled( ) ) { s_game_active = false; s_game_result = s_server_game.status == "push" ? 2 : s_server_game.status == "won" || s_server_game.status == "blackjack" ? 1 : -1; s_game_result_at = animation_time; s_result_tab = s_gamble_tab; }
					}
						else if ( s_gamble_tab == 3 ) {
							s_ck_crossed = 0;
							s_ck_caught = false;
							s_ck_server_caught = false;
						s_ck_hopping = false;
						s_ck_hit_lane = -1;
						s_ck_hit_progress = 0.0f;
						}
						else if ( s_gamble_tab == 4 ) {
							s_crash_point = static_cast< float >( s_server_game.crash_point );
							s_crash_time_frac = ( std::clamp )( std::pow( ( std::max )( 0.0f, ( s_crash_point - 1.0f ) / 12.0f ), 1.0f / 1.6f ), 0.05f, 1.0f );
						s_crash_mult = 1.0f;
						s_crash_exploded = false;
						}
					}
				}
			}
			else if ( s_profile_page_tab == 2 )
			{
				const float animation_time = std::chrono::duration< float >(
					std::chrono::steady_clock::now( ).time_since_epoch( ) ).count( );

				static float s_cr_prev_time = 0.0f;
				const float cr_dt = ( s_cr_prev_time > 0.0f )
					? min( animation_time - s_cr_prev_time, 0.1f )
					: 0.016f;
				s_cr_prev_time = animation_time;

				// ── Layout ────────────────────────────────────────────────────────
				const float arena_x = page_x;
				const float arena_w = page_w;
				const float arena_y = tabs_y + tab_h + 12.0f;
				const float full_bot = ( content_y - 2.0f + panel_h ) - 20.0f;
				const float arena_h = ( full_bot - arena_y ) * 0.5f;
				const float arena_bot = arena_y + arena_h;
				const float cr_cx = arena_x + arena_w * 0.5f;

				// ── Arena background ───────────────────────────────────────────────
				bypass::overlay::filled_rounded_rectangle(
					{ arena_x, arena_y, arena_w, arena_h }, 12.0f,
					bypass::overlay::color_t { 22, 19, 35, 190 } );

				bypass::overlay::rounded_rectangle(
					{ arena_x, arena_y, arena_w, arena_h }, 12.0f, col_border, 1.0f );

				// ── Tier + feature definitions ─────────────────────────────────────
				struct cr_tier_t {
					bypass::overlay::color_t col;
					bypass::overlay::color_t col_bg;
					bypass::overlay::color_t col_bd;
					const wchar_t* label;
				};
				static constexpr cr_tier_t cr_tiers[ 3 ] = {
					{ {  61, 200, 122, 215 }, {  61, 200, 122, 110 }, {  61, 200, 122, 130 }, L"COMMON"    },
					{ {  91, 146, 255, 215 }, {  91, 146, 255, 110 }, {  91, 146, 255, 130 }, L"RARE"      },
					{ { 245, 194,  48, 215 }, { 245, 194,  48, 110 }, { 245, 194,  48, 130 }, L"LEGENDARY" },
				};

				struct cr_feat_t { const wchar_t* name; int tier; };
				static constexpr cr_feat_t cr_feats[ 12 ] = {
					{ L"Spin Bot",     1 }, { L"Rapid Fire",   0 }, { L"Wall Pierce",  2 },
					{ L"Silent Aim",   2 }, { L"FOV Changer",  1 }, { L"Car Fly",      0 },
					{ L"Inf. Fuel",    0 }, { L"Chams",        2 }, { L"No Spread",    1 },
					{ L"No Recoil",    1 }, { L"Magic Bullet", 2 }, { L"Fire Rate",    0 },
				};
				constexpr int cr_seq_len = 12;

				// ── Reel geometry ──────────────────────────────────────────────────
				constexpr float item_w = 82.0f;
				constexpr float item_gap = 5.0f;
				constexpr float per_item = item_w + item_gap;
				const float     item_h = arena_h - 16.0f;
				const float     reel_y = arena_y + 8.0f;

				// ── Spin state ─────────────────────────────────────────────────────
				static float s_cr_pos = 0.0f;
				static float s_cr_vel = 38.0f;
				static bool  s_cr_spin = false;
				static float s_cr_target = 0.0f;
				static float s_cr_start = 0.0f;
				static int   s_cr_feat = -1;

				if ( s_cr_spin ) {
					s_cr_pos += s_cr_vel * cr_dt;
					const float total = s_cr_target - s_cr_start;
					const float prog = total > 0.0f
						? min( ( s_cr_pos - s_cr_start ) / total, 1.0f ) : 1.0f;

					if ( prog < 0.72f ) {
						s_cr_vel = 4200.0f;
					}
					else {
						const float t = ( prog - 0.72f ) / 0.28f;
						s_cr_vel = max( 4200.0f * ( 1.0f - t * t ), 28.0f );
					}

					if ( s_cr_pos >= s_cr_target ) {
						s_cr_pos = s_cr_target;
						s_cr_vel = 38.0f;
						s_cr_spin = false;
						s_crate_last_result = s_cr_feat;
						s_crate_unlocked[ s_cr_feat ] = true;
						// Casino unlock → real exploit config bool.
						// The gambling side previously only updated the
						// cosmetic s_crate_unlocked[] indicator; the
						// actual exploit toggles in config::exploits
						// stayed false forever, so winning "Spin Bot"
						// on the crate never enabled anything ingame.
						// Index order below MUST match feature_ids[]
						// declared right above in this scope (that's
						// where s_cr_feat comes from).
						switch ( s_cr_feat ) {
						case 0:  config::exploits::spin_bot      = true; break;
						case 1:  config::exploits::rapid_fire    = true; break;
						case 2:  config::exploits::wall_pierce   = true; break;
						case 3:  config::exploits::silent_aim    = true; break;
						case 4:  config::exploits::fov_changer   = true; break;
						case 5:  config::exploits::car_fly       = true; break;
						case 6:  config::exploits::infinite_fuel = true; break;
						case 7:  config::exploits::chams         = true; break;
						case 8:  config::exploits::no_spread     = true; break;
						case 9:  config::exploits::no_recoil     = true; break;
						case 10: config::exploits::magic_bullet  = true; break;
						case 11: config::exploits::fire_rate     = true; break;
						default: break;
						}
						s_splash_active = true;
						s_splash_start = animation_time;
					}
				}
				else {
					s_cr_pos += 38.0f * cr_dt;
				}

				// ── Splash → collection handoff ────────────────────────────────────
				constexpr float splash_dur = 2.5f;
				if ( s_splash_active && animation_time >= s_splash_start + splash_dur ) {
					s_splash_active = false;
					s_crate_result_until = animation_time + 2.5f;
				}
				const float splash_t = s_splash_active
					? min( ( animation_time - s_splash_start ) / splash_dur, 1.0f )
					: 0.0f;

				// ── Reel rendering — horizontal, clipped at arena left/right ──────
				const int first_ti = static_cast< int >( ( s_cr_pos - item_w ) / per_item ) - 1;
				const int last_ti = static_cast< int >( ( s_cr_pos + arena_w + item_w ) / per_item ) + 1;

				for ( int ti = first_ti; ti <= last_ti; ++ti )
				{
					const float tx = arena_x + static_cast< float >( ti ) * per_item - s_cr_pos;
					const float tcx = tx + item_w * 0.5f;

					if ( tx + item_w <= arena_x ) continue;
					if ( tx >= arena_x + arena_w ) break;

					const int fi = ( ( ti % cr_seq_len ) + cr_seq_len * 100 ) % cr_seq_len;
					const auto& feat = cr_feats[ fi ];
					const auto& tier = cr_tiers[ feat.tier ];

					const bool  at_center = std::abs( tcx - cr_cx ) < item_w * 0.5f;
					const float arena_right = arena_x + arena_w;

					const float vis_x = max( tx, arena_x );
					const float vis_r = min( tx + item_w, arena_right );
					const float vis_w = vis_r - vis_x;
					const bool  full = vis_w >= item_w - 0.5f;

					auto hclip = [ arena_x, arena_right ]( float rx, float rw )
						-> std::pair< float, float >
						{
							const float cx = max( rx, arena_x );
							const float cw = min( rx + rw, arena_right ) - cx;
							return { cx, cw > 0.0f ? cw : 0.0f };
						};

					// Background + glass highlight
					if ( full ) {
						bypass::overlay::filled_rounded_rectangle(
							{ tx, reel_y, item_w, item_h }, 8.0f, tier.col_bg );
						bypass::overlay::filled_rounded_rectangle(
							{ tx + 1.5f, reel_y + 1.5f, item_w - 3.0f, item_h * 0.35f }, 6.0f,
							{ 255, 255, 255, static_cast< std::uint8_t >( at_center ? 15 : 8 ) } );
						bypass::overlay::rounded_rectangle(
							{ tx, reel_y, item_w, item_h }, 8.0f, tier.col_bd, 1.0f );
					}
					else {
						bypass::overlay::filled_rectangle(
							{ vis_x, reel_y, vis_w, item_h }, tier.col_bg );
						auto [gx, gw] = hclip( tx + 1.5f, item_w - 3.0f );
						if ( gw > 0.0f )
							bypass::overlay::filled_rectangle(
								{ gx, reel_y + 1.5f, gw, item_h * 0.35f },
								{ 255, 255, 255, static_cast< std::uint8_t >( at_center ? 15 : 8 ) } );
					}

					// Top accent line
					{
						auto [px, pw] = hclip( tx + item_w * 0.25f, item_w * 0.5f );
						if ( pw > 0.0f )
							bypass::overlay::filled_rectangle(
								{ px, reel_y + 2.0f, pw, 2.0f },
								{ tier.col.r, tier.col.g, tier.col.b,
								  static_cast< std::uint8_t >( at_center ? 255 : 120 ) } );
					}

					// Circles only: can't clip geometry, keep tcx guard
					if ( tcx > arena_x && tcx < arena_right ) {
						const float emblem_cy = reel_y + item_h * 0.38f;
						if ( at_center )
							bypass::overlay::filled_circle( { tcx, emblem_cy }, 22.0f,
								{ tier.col.r, tier.col.g, tier.col.b, 35 } );
						bypass::overlay::filled_circle( { tcx, emblem_cy }, 14.0f,
							{ tier.col.r, tier.col.g, tier.col.b,
							  static_cast< std::uint8_t >( at_center ? 50 : 20 ) } );
					}

					// Icon, name, pill — scissor-clipped to arena bounds
					bypass::overlay::push_clip( { arena_x, reel_y, arena_w, item_h } );

					bypass::overlay::text_ex( L"\uf005",
						{ tx, reel_y + item_h * 0.15f, item_w, item_h * 0.45f },
						{ tier.col.r, tier.col.g, tier.col.b,
						  static_cast< std::uint8_t >( at_center ? 255 : 160 ) },
						at_center ? 26.0f : 20.0f,
						bypass::overlay::text_weight_t::regular,
						L"Font Awesome", bypass::overlay::text_align_t::center, false );

					bypass::overlay::text_ex( feat.name,
						{ tx + 4.0f, reel_y + item_h * 0.62f, item_w - 8.0f, item_h * 0.18f },
						{ 245, 245, 250, static_cast< std::uint8_t >( at_center ? 255 : 150 ) },
						10.0f, bypass::overlay::text_weight_t::bold,
						L"Inter-Bold", bypass::overlay::text_align_t::center, false );

					{
						const float pill_w = item_w * 0.75f;
						const float pill_h = 14.0f;
						const float pill_x = tx + ( item_w - pill_w ) * 0.5f;
						const float pill_y = reel_y + item_h * 0.82f;
						bypass::overlay::filled_rounded_rectangle(
							{ pill_x, pill_y, pill_w, pill_h }, pill_h * 0.5f,
							{ tier.col.r, tier.col.g, tier.col.b,
							  static_cast< std::uint8_t >( at_center ? 40 : 15 ) } );
						bypass::overlay::text_ex( tier.label,
							{ pill_x, pill_y, pill_w, pill_h },
							{ tier.col.r, tier.col.g, tier.col.b,
							  static_cast< std::uint8_t >( at_center ? 255 : 140 ) },
							7.5f, bypass::overlay::text_weight_t::black,
							L"Geist-Bold", bypass::overlay::text_align_t::center, false );
					}

					bypass::overlay::pop_clip( );
					// Selected highlight — only when fully inside
					if ( at_center && full ) {
						bypass::overlay::rounded_rectangle(
							{ tx - 2.0f, reel_y - 2.0f, item_w + 4.0f, item_h + 4.0f }, 9.0f,
							{ col_accent.r, col_accent.g, col_accent.b, 50 }, 2.0f );
						bypass::overlay::rounded_rectangle(
							{ tx, reel_y, item_w, item_h }, 8.0f,
							{ col_accent.r, col_accent.g, col_accent.b, 230 }, 1.5f );
					}


				}

				// ── Spin pill button ─────────────────────────────────────────────
				constexpr float btn_r = 34.0f;              // layout anchor only
				const float btn_cy = arena_bot + btn_r + 12.0f;
				const float btn_dx = mx - cr_cx;
				const float btn_dy = my - btn_cy;
				constexpr float pill_h = 52.0f;
				const float     pill_w = page_w - 60.0f;
				const float     pill_x = cr_cx - pill_w * 0.5f;
				const float     pill_y = btn_cy - pill_h * 0.5f;
				const bool  btn_hov = std::abs( btn_dx ) <= pill_w * 0.5f
					&& std::abs( btn_dy ) <= pill_h * 0.5f;
				const bool  btn_dis = !crisis::casino::client.ready( ) || s_cr_spin || s_splash_active || s_shop_coins < 15u;

				if ( clicked && btn_hov && !btn_dis ) {
					const auto crate_result = crisis::casino::client.crate( 15 );
					static constexpr const char* feature_ids[ ] = { "spin_bot", "rapid_fire", "wall_pierce", "silent_aim", "fov_changer", "car_fly", "infinite_fuel", "chams", "no_spread", "no_recoil", "magic_bullet", "fire_rate" };
					if ( !apply_casino_result( crate_result ) ) return;
					s_cr_feat = 0; for ( int i = 0; i < cr_seq_len; ++i ) if ( crate_result.feature == feature_ids[ i ] ) { s_cr_feat = i; break; }
					const float center_offset = ( arena_w - item_w ) * 0.5f;
					const int   cur_ti = static_cast< int >( ( s_cr_pos + center_offset ) / per_item );
					const int   ahead = cur_ti + 5 * cr_seq_len + cr_seq_len;
					const int   rem = ( ( ahead % cr_seq_len ) + cr_seq_len * 200 ) % cr_seq_len;
					const int   target_ti = ahead + ( s_cr_feat - rem + cr_seq_len ) % cr_seq_len;
					s_cr_start = s_cr_pos;
					s_cr_target = static_cast< float >( target_ti ) * per_item - center_offset;
					s_cr_vel = 4200.0f;
					s_cr_spin = true;
				}

				// ── Pill draw ─────────────────────────────────────────────────────
				{
					const bypass::overlay::color_t pill_bg = btn_dis
						? bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 70 }
						: btn_hov
						? bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 230 }
					: bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 205 };

					bypass::overlay::filled_rounded_rectangle(
						{ pill_x, pill_y, pill_w, pill_h }, 26.0f, pill_bg );

					//bypass::overlay::push_clip( { pill_x, pill_y, pill_w, pill_h * 0.48f } );
					//bypass::overlay::filled_rounded_rectangle(
					//	{ pill_x, pill_y, pill_w, pill_h }, 26.0f,
					//	bypass::overlay::color_t { 255, 255, 255, static_cast< std::uint8_t >( btn_dis ? 12 : 22 ) } );
					//bypass::overlay::pop_clip( );

					const float sec_w = pill_w / 3.0f;
					const float div_h = 28.0f;
					const float div_y = btn_cy - div_h * 0.5f;
					const auto  div_a = static_cast< std::uint8_t >( btn_dis ? 25 : 50 );
					bypass::overlay::filled_rectangle(
						{ pill_x + sec_w,         div_y, 1.0f, div_h },
						bypass::overlay::color_t { 255, 255, 255, div_a } );
					bypass::overlay::filled_rectangle(
						{ pill_x + sec_w * 2.0f,  div_y, 1.0f, div_h },
						bypass::overlay::color_t { 255, 255, 255, div_a } );

					const bypass::overlay::color_t col_label { 255, 255, 255,
						static_cast< std::uint8_t >( btn_dis ? 60 : 130 ) };
					const bypass::overlay::color_t col_val { 255, 255, 255,
						static_cast< std::uint8_t >( btn_dis ? 90 : 230 ) };
					const bypass::overlay::color_t col_gold = btn_dis
						? bypass::overlay::color_t { 180, 165, 100, 100 }
					: bypass::overlay::color_t { 249, 217,  74, 230 };
					const bypass::overlay::color_t col_theme = btn_dis
						? bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 100 }
					: bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 230 };

					// Left section: COST + 15
					const float ls_cx = pill_x + sec_w * 0.5f;
					bypass::overlay::text_ex( L"COST",
						{ ls_cx - 30.0f, btn_cy - 16.0f, 60.0f, 11.0f },
						col_label, 8.5f, bypass::overlay::text_weight_t::bold,
						L"Inter-Bold", bypass::overlay::text_align_t::center, false );
					bypass::overlay::text_ex( L"15",
						{ ls_cx - 30.0f, btn_cy - 1.0f, 60.0f, 16.0f },
						col_gold, 14.0f, bypass::overlay::text_weight_t::black,
						L"Inter-Bold", bypass::overlay::text_align_t::center, false );

					// Center section: SPIN + subtitle (centered as a group in pill_h=52)
					bypass::overlay::text_ex( L"SPIN",
						{ cr_cx - 50.0f, btn_cy - 14.0f, 100.0f, 15.0f },
						col_val, 13.0f, bypass::overlay::text_weight_t::black,
						L"Inter-Bold", bypass::overlay::text_align_t::center, false );
					bypass::overlay::text_ex( L"the wheel",
						{ cr_cx - 40.0f, btn_cy + 3.0f, 80.0f, 11.0f },
						col_label, 8.5f, bypass::overlay::text_weight_t::bold,
						L"Inter-Bold", bypass::overlay::text_align_t::center, false );

					// Right section: BALANCE + coin count. Zero coins
					// reads as "BROKE" — matches the coin-bar and the
					// profile-page currency cards.
					const float rs_cx = pill_x + sec_w * 2.5f;
					const auto  coins_str = ( s_shop_coins == 0 )
						? std::wstring( L"BROKE" )
						: std::to_wstring( s_shop_coins );
					bypass::overlay::text_ex( L"BALANCE",
						{ rs_cx - 35.0f, btn_cy - 16.0f, 70.0f, 11.0f },
						col_label, 8.5f, bypass::overlay::text_weight_t::bold,
						L"Inter-Bold", bypass::overlay::text_align_t::center, false );
					bypass::overlay::text_ex( coins_str.c_str( ),
						{ rs_cx - 30.0f, btn_cy - 1.0f, 60.0f, 16.0f },
						col_theme, 14.0f, bypass::overlay::text_weight_t::black,
						L"Inter-Bold", bypass::overlay::text_align_t::center, false );
				}
				// ── Bottom layout: Collection + Drop Rates ──────────────────────
				const float bottom_y = btn_cy + btn_r + 14.0f;
				const float bottom_h = full_bot - bottom_y;
				const float bottom_gap = 12.0f;
				const float left_w = page_w * 0.62f;
				const float right_w = page_w - left_w - bottom_gap;

				// ── LEFT: Collection Grid ───────────────────────────────────
				bypass::overlay::filled_rounded_rectangle(
					{ page_x, bottom_y, left_w, bottom_h }, 10.0f,
					bypass::overlay::color_t { 22, 19, 35, 160 } );
				bypass::overlay::rounded_rectangle(
					{ page_x, bottom_y, left_w, bottom_h }, 10.0f, col_border, 1.0f );

				// Header
				bypass::overlay::text_ex( L"COLLECTION",
					{ page_x + 14.0f, bottom_y + 10.0f, 120.0f, 14.0f },
					bypass::overlay::color_t { 213, 208, 225, 105 }, 10.0f,
					bypass::overlay::text_weight_t::semi_bold,
					L"Inter-Medium", bypass::overlay::text_align_t::left, false );

				// Unlock count
				int unlocked_count = 0;
				for ( int i = 0; i < 12; ++i )
					if ( s_crate_unlocked[ i ] ) ++unlocked_count;

				wchar_t count_buf[ 16 ];
				std::swprintf( count_buf, 16, L"%d / 12", unlocked_count );
				bypass::overlay::text_ex( count_buf,
					{ page_x + left_w - 80.0f, bottom_y + 10.0f, 66.0f, 14.0f },
					bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 200 }, 10.0f,
					bypass::overlay::text_weight_t::bold,
					L"Inter-Bold", bypass::overlay::text_align_t::right, false );

				// Feature tiles — 6 per row
				constexpr float tile_gap = 5.0f;
				const float tile_area_x = page_x + 10.0f;
				const float tile_area_y = bottom_y + 32.0f;
				const float tile_w = ( left_w - 20.0f - tile_gap * 5.0f ) / 6.0f;
				const float tile_h = ( bottom_h - 46.0f - tile_gap ) * 0.5f;

				for ( int i = 0; i < 12; ++i ) {
					const int col = i % 6;
					const int row = i / 6;
					const float tx = tile_area_x + col * ( tile_w + tile_gap );
					const float ty = tile_area_y + row * ( tile_h + tile_gap );
					const bool unlocked = s_crate_unlocked[ i ];
					const auto& tier = cr_tiers[ cr_feats[ i ].tier ];
					const bool  is_result = ( s_crate_last_result == i && animation_time < s_crate_result_until );
					const float result_t = is_result ? ( animation_time - ( s_crate_result_until - 2.5f ) ) / 2.5f : 0.0f;
					auto fclamp = [ ]( float v ) noexcept { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; };

					if ( is_result ) {
						// Phase alphas (0\u21921 floats)
						const float lock_a = fclamp( 1.0f - result_t / 0.35f );
						const float unlock_a = fclamp( result_t / 0.35f ) * fclamp( 1.0f - ( result_t - 0.65f ) / 0.35f );
						const float star_a = fclamp( ( result_t - 0.65f ) / 0.35f );
						const float bg_t = fclamp( ( result_t - 0.2f ) / 0.4f );

						// Background: lerp locked-dark \u2192 tier color
						bypass::overlay::filled_rounded_rectangle(
							{ tx, ty, tile_w, tile_h }, 6.0f, bypass::overlay::color_t {
								static_cast< std::uint8_t >( 15 + ( tier.col.r - 15 ) * bg_t ),
								static_cast< std::uint8_t >( 14 + ( tier.col.g - 14 ) * bg_t ),
								static_cast< std::uint8_t >( 28 + ( tier.col.b - 28 ) * bg_t ),
								static_cast< std::uint8_t >( 180 - 155.0f * bg_t ) } );
						bypass::overlay::rounded_rectangle(
							{ tx, ty, tile_w, tile_h }, 6.0f,
							bypass::overlay::color_t { tier.col.r, tier.col.g, tier.col.b,
								static_cast< std::uint8_t >( 55 + 35.0f * bg_t ) }, 1.5f );

						// Lock fading out
						if ( lock_a > 0.01f )
							bypass::overlay::text_ex( L"\uf023",
								{ tx, ty, tile_w, tile_h - 14.0f },
								bypass::overlay::color_t { 120, 115, 145, static_cast< std::uint8_t >( lock_a * 200 ) },
								11.0f, bypass::overlay::text_weight_t::regular,
								L"Font Awesome", bypass::overlay::text_align_t::center, false );
						// Unlock-open fading in then out
						if ( unlock_a > 0.01f )
							bypass::overlay::text_ex( L"\uf3c1",
								{ tx, ty, tile_w, tile_h - 14.0f },
								bypass::overlay::color_t { tier.col.r, tier.col.g, tier.col.b,
									static_cast< std::uint8_t >( unlock_a * 220 ) },
								12.0f, bypass::overlay::text_weight_t::regular,
								L"Font Awesome", bypass::overlay::text_align_t::center, false );
						// Star fading in
						if ( star_a > 0.01f )
							bypass::overlay::text_ex( L"\uf005",
								{ tx, ty, tile_w, tile_h - 14.0f },
								bypass::overlay::color_t { tier.col.r, tier.col.g, tier.col.b,
									static_cast< std::uint8_t >( star_a * 220 ) },
								12.0f, bypass::overlay::text_weight_t::regular,
								L"Font Awesome", bypass::overlay::text_align_t::center, false );
					}
					else {
						// Tile background (normal)
						bypass::overlay::filled_rounded_rectangle(
							{ tx, ty, tile_w, tile_h }, 6.0f,
							unlocked
							? bypass::overlay::color_t { tier.col.r, tier.col.g, tier.col.b, 25 }
						: bypass::overlay::color_t { 15, 14, 28, 180 } );
						bypass::overlay::rounded_rectangle(
							{ tx, ty, tile_w, tile_h }, 6.0f,
							unlocked
							? bypass::overlay::color_t { tier.col.r, tier.col.g, tier.col.b, 90 }
						: bypass::overlay::color_t { 55, 55, 55, 120 }, 1.0f );

						if ( unlocked ) {
							bypass::overlay::text_ex( L"\uf005",
								{ tx, ty, tile_w, tile_h - 14.0f },
								{ tier.col.r, tier.col.g, tier.col.b, 220 },
								12.0f, bypass::overlay::text_weight_t::regular,
								L"Font Awesome", bypass::overlay::text_align_t::center, false );
						}
						else {
							bypass::overlay::text_ex( L"\uf023",
								{ tx, ty, tile_w, tile_h - 14.0f },
								bypass::overlay::color_t { 120, 115, 145, 200 },
								11.0f, bypass::overlay::text_weight_t::regular,
								L"Font Awesome", bypass::overlay::text_align_t::center, false );
						}
					}

					// Feature name
					bypass::overlay::text_ex( cr_feats[ i ].name,
						{ tx + 2.0f, ty + tile_h - 15.0f, tile_w - 4.0f, 13.0f },
						unlocked
						? bypass::overlay::color_t { 244, 241, 250, 220 }
						: bypass::overlay::color_t { 200, 196, 215, 200 },
						7.5f, bypass::overlay::text_weight_t::black,
						L"Inter-Bold", bypass::overlay::text_align_t::center, false );
				}

				// -- RIGHT: Drop Rates -- column bars
				const float right_x = page_x + left_w + bottom_gap;

				bypass::overlay::filled_rounded_rectangle(
					{ right_x, bottom_y, right_w, bottom_h }, 10.0f,
					bypass::overlay::color_t { 22, 19, 35, 160 } );
				bypass::overlay::rounded_rectangle(
					{ right_x, bottom_y, right_w, bottom_h }, 10.0f, col_border, 1.0f );

				bypass::overlay::text_ex( L"DROP RATES",
					{ right_x + 14.0f, bottom_y + 10.0f, 120.0f, 14.0f },
					bypass::overlay::color_t { 213, 208, 225, 105 }, 10.0f,
					bypass::overlay::text_weight_t::semi_bold,
					L"Inter-Medium", bypass::overlay::text_align_t::left, false );

				struct rate_col_t { const wchar_t* label; float pct; bypass::overlay::color_t col; };
				static constexpr rate_col_t rates[ ] = {
					{ L"Common",    0.60f, {  61, 200, 122, 215 } },
					{ L"Rare",      0.30f, {  91, 146, 255, 215 } },
					{ L"Legendary", 0.10f, { 245, 194,  48, 215 } },
				};

				// Column geometry
				constexpr float pct_h = 14.0f;
				constexpr float name_h = 12.0f;
				constexpr float col_gap = 8.0f;
				const float cols_x = right_x + 14.0f;
				const float cols_w = right_w - 28.0f;
				const float col_w = ( cols_w - col_gap * 2.0f ) / 3.0f;
				const float bar_top = bottom_y + 32.0f + pct_h + 5.0f;
				const float bar_bot = bottom_y + bottom_h - 12.0f - name_h - 5.0f;
				const float bar_area_h = bar_bot - bar_top;
				constexpr float max_pct = 0.60f;
				const float bar_radius = ( std::min ) ( col_w * 0.35f, 5.0f );

				for ( int ci = 0; ci < 3; ++ci ) {
					const auto& r = rates[ ci ];
					const float cx = cols_x + ci * ( col_w + col_gap );
					const float bar_h = bar_area_h * ( r.pct / max_pct );
					const float bar_y = bar_bot - bar_h;

					// Track
					bypass::overlay::filled_rounded_rectangle(
						{ cx, bar_top, col_w, bar_area_h }, bar_radius,
						{ 255, 255, 255, 10 } );

					// Fill
					bypass::overlay::filled_rounded_rectangle(
						{ cx, bar_y, col_w, bar_h }, bar_radius,
						{ r.col.r, r.col.g, r.col.b, 185 } );

					// Top highlight
					if ( bar_h > 6.0f )
						bypass::overlay::filled_rounded_rectangle(
							{ cx + 2.0f, bar_y + 2.0f, col_w - 4.0f,
							  ( std::min ) ( bar_h * 0.35f, 10.0f ) }, bar_radius,
							{ 255, 255, 255, 28 } );

					// Percentage above bar
					wchar_t pct_buf[ 8 ];
					std::swprintf( pct_buf, 8, L"%.0f%%", r.pct * 100.0f );
					bypass::overlay::text_ex( pct_buf,
						{ cx, bottom_y + 32.0f, col_w, pct_h },
						{ r.col.r, r.col.g, r.col.b, 240 }, 10.0f,
						bypass::overlay::text_weight_t::black,
						L"Geist-Bold", bypass::overlay::text_align_t::center, false );

					// Tier name below bar
					bypass::overlay::text_ex( r.label,
						{ cx - 2.0f, bar_bot + 5.0f, col_w + 4.0f, name_h },
						{ 220, 216, 232, 200 }, 8.5f,
						bypass::overlay::text_weight_t::black,
						L"Inter-Bold", bypass::overlay::text_align_t::center, false );
				}

				// ── Splash overlay — concept A: scale-zoom + rotating rays ─────────
				if ( s_splash_active && s_cr_feat >= 0 ) {
					auto sp_fc = [ ]( float v ) noexcept { return v < 0.f ? 0.f : v > 1.f ? 1.f : v; };

					const auto& sp_feat = cr_feats[ s_cr_feat ];
					const auto& sp_tier = cr_tiers[ sp_feat.tier ];

					const float splash_area_h = full_bot - arena_y;
					const float reel_cy = arena_y + arena_h * 0.5f;
					const float page_cy = arena_y + splash_area_h * 0.5f;

					// Dim — snaps in fast, holds, fades last 14 %
					const float dim_a = sp_fc( splash_t * 7.0f )
						* sp_fc( 1.0f - ( splash_t - 0.86f ) / 0.14f );

					bypass::overlay::push_clip( { page_x, arena_y, page_w, splash_area_h } );

					bypass::overlay::filled_rectangle(
						{ page_x, arena_y, page_w, splash_area_h },
						{ 6, 4, 16, static_cast< std::uint8_t >( dim_a * 215 ) } );

					// Card — easeOutBack scale, rises from reel center to page center
					const float rise_t = sp_fc( splash_t / 0.42f );
					constexpr float c1 = 1.70158f;
					constexpr float c3 = c1 + 1.0f;
					const float card_scale = rise_t < 1.0f
						? 1.0f + c3 * ( rise_t - 1.0f ) * ( rise_t - 1.0f ) * ( rise_t - 1.0f )
						+ c1 * ( rise_t - 1.0f ) * ( rise_t - 1.0f )
						: 1.0f;
					const float card_fade = sp_fc( splash_t * 9.0f )
						* sp_fc( 1.0f - ( splash_t - 0.85f ) / 0.15f );

					constexpr float card_w = 158.0f;
					constexpr float card_h = 196.0f;
					const float cw = card_w * card_scale;
					const float ch = card_h * card_scale;
					const float ccx = cr_cx;
					const float ccy = reel_cy + ( page_cy - reel_cy ) * rise_t;
					const float cx = ccx - cw * 0.5f;
					const float cy = ccy - ch * 0.5f;

					if ( card_fade > 0.01f && card_scale > 0.06f ) {
						const std::uint8_t bg_a = static_cast< std::uint8_t >( card_fade * 235 );
						const std::uint8_t bd_a = static_cast< std::uint8_t >( card_fade * 170 );
						const float        ra = ( std::min ) ( 10.0f * card_scale, 12.0f );

						// Background
						bypass::overlay::filled_rounded_rectangle(
							{ cx, cy, cw, ch }, ra,
							{ static_cast< std::uint8_t >( sp_tier.col.r / 5 ),
							  static_cast< std::uint8_t >( sp_tier.col.g / 5 ),
							  static_cast< std::uint8_t >( sp_tier.col.b / 5 ), bg_a } );
						// Tier border
						bypass::overlay::rounded_rectangle(
							{ cx, cy, cw, ch }, ra,
							{ sp_tier.col.r, sp_tier.col.g, sp_tier.col.b, bd_a }, 2.0f );
						// Glass highlight
						bypass::overlay::filled_rounded_rectangle(
							{ cx + 3.0f, cy + 3.0f, cw - 6.0f, ch * 0.30f }, ra * 0.7f,
							{ 255, 255, 255, static_cast< std::uint8_t >( card_fade * 11 ) } );

						// Text — only once card is big enough to read
						if ( card_scale > 0.42f ) {
							const std::uint8_t tx_a = static_cast< std::uint8_t >(
								card_fade * sp_fc( ( card_scale - 0.42f ) / 0.25f ) * 225 );
							// Star icon (Font Awesome solid star )
							bypass::overlay::text_ex( L"",
								{ cx, cy, cw, ch - 44.0f * card_scale },
								{ sp_tier.col.r, sp_tier.col.g, sp_tier.col.b, tx_a },
								26.0f, bypass::overlay::text_weight_t::regular,
								L"Font Awesome", bypass::overlay::text_align_t::center, false );
							// Feature name
							bypass::overlay::text_ex( sp_feat.name,
								{ cx + 4.0f, ccy + ch * 0.28f, cw - 8.0f, 18.0f },
								{ 244, 241, 250, tx_a },
								12.0f, bypass::overlay::text_weight_t::black,
								L"Inter-Bold", bypass::overlay::text_align_t::center, false );
							// Tier label
							bypass::overlay::text_ex( sp_tier.label,
								{ cx + 4.0f, ccy + ch * 0.28f + 18.0f, cw - 8.0f, 14.0f },
								{ sp_tier.col.r, sp_tier.col.g, sp_tier.col.b,
								  static_cast< std::uint8_t >( card_fade * 170 ) },
								7.5f, bypass::overlay::text_weight_t::black,
								L"Inter-Bold", bypass::overlay::text_align_t::center, false );
						}
					}

					bypass::overlay::pop_clip( );
				}
			}
			// Hero card
			const float hero_x = side_x + 12.0f;
			const float hero_y = side_y + 12.0f;
			const float hero_w = side_w - 24.0f;
			constexpr float hero_h = 170.0f;

			bypass::overlay::filled_rounded_rectangle(
				{ hero_x, hero_y, hero_w, hero_h }, 12.0f,
				bypass::overlay::color_t { 26, 23, 42, 220 } );

			// Dot pattern background animation
			{
				constexpr auto dot_spacing = 20.0f;
				constexpr auto outer_dot_radius = 3.0f;
				constexpr auto inner_dot_radius = 2.0f;
				const bypass::overlay::point_t pattern_center { hero_w * 0.5f, hero_h * 0.5f };
				const auto maximum_distance = std::sqrt(
					pattern_center.x * pattern_center.x + pattern_center.y * pattern_center.y );
				const auto animation_time = std::chrono::duration<float>(
					std::chrono::steady_clock::now( ).time_since_epoch( ) ).count( );
				for ( auto dy_pos = 0.0f; dy_pos < hero_h; dy_pos += dot_spacing ) {
					for ( auto dx_pos = 0.0f; dx_pos < hero_w; dx_pos += dot_spacing ) {
						const auto dx = dx_pos - pattern_center.x;
						const auto dy = dy_pos - pattern_center.y;
						const auto distance = std::sqrt( dx * dx + dy * dy );
						const auto distance_fade = ( std::max ) ( 0.0f, 1.0f - distance / maximum_distance );
						const auto pulse = 0.5f + 0.5f * std::sin(
							animation_time * 2.0f + ( dx_pos + dy_pos ) * 0.05f );
						const auto alpha = 0.22f * distance_fade * pulse;
						if ( alpha <= 0.005f )
							continue;
						const auto alpha_byte = static_cast< std::uint8_t >( alpha * 255.0f );
						const bypass::overlay::point_t dot { hero_x + dx_pos + 1.0f, hero_y + dy_pos + 1.0f };
						bypass::overlay::filled_circle(
							dot, outer_dot_radius, bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, alpha_byte } );
						bypass::overlay::filled_circle(
							dot, inner_dot_radius, bypass::overlay::color_t { 18, 12, 36, alpha_byte } );
					}
				}
			}

			const float avatar_cx = hero_x + hero_w * 0.5f;
			const float avatar_cy = hero_y + 58.0f;
			constexpr float avatar_r = 34.0f;

			// Aurora "gradient" behind the profile — two diagonal blooms anchored
			// to the avatar's top-left / bottom-right, breathing on offset phases.
			// Drawn after the hero fill, before the border + avatar.
			const float glow_time = std::chrono::duration<float>(
				std::chrono::steady_clock::now( ).time_since_epoch( ) ).count( );
			const float glow_breath_tl = 0.85f + 0.15f * std::sin( glow_time * 1.4f );
			const float glow_breath_br = 0.85f + 0.15f * std::sin( glow_time * 1.4f + 2.1f );

			const auto draw_glow = [ ]( float cx, float cy, float inner_r, float spread,
				std::uint8_t r, std::uint8_t g, std::uint8_t b, float peak ) {
					constexpr int steps = 6;
					for ( int i = steps; i >= 1; --i ) {
						const float t = static_cast< float >( i ) / static_cast< float >( steps );
						const auto alpha = static_cast< std::uint8_t >(
							peak * ( 1.0f - t ) * ( 1.0f - t ) );
						if ( alpha == 0 )
							continue;
						bypass::overlay::filled_circle( { cx, cy }, inner_r + spread * t,
							bypass::overlay::color_t { r, g, b, alpha } );
					}
				};

			bypass::overlay::rounded_rectangle(
				{ hero_x, hero_y, hero_w, hero_h }, 12.0f,
				bypass::overlay::color_t { 70, 66, 92, 90 }, 1.0f );

			// Level ring + avatar
			bypass::overlay::filled_circle( { avatar_cx, avatar_cy }, avatar_r + 4.0f,
				bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 210 } );
			bypass::overlay::filled_circle( { avatar_cx, avatar_cy }, avatar_r + 1.5f,
				bypass::overlay::color_t { 26, 23, 42, 255 } );

			if ( s_profile_image_registered ) {
				bypass::overlay::image( profile_image_id,
					{ avatar_cx - avatar_r, avatar_cy - avatar_r, avatar_r * 2.0f, avatar_r * 2.0f },
					bypass::overlay::color_t { 255, 255, 255, 255 }, 1.0f, avatar_r );
			}
			else {
				bypass::overlay::filled_circle( { avatar_cx, avatar_cy }, avatar_r,
					bypass::overlay::color_t { 34, 30, 54, 255 } );

				bypass::overlay::text_ex( L"",
					{ avatar_cx - avatar_r, avatar_cy - avatar_r, avatar_r * 2.0f, avatar_r * 2.0f },
					col_accent, 26.0f, bypass::overlay::text_weight_t::regular,
					L"Font Awesome", bypass::overlay::text_align_t::center, false );
			}

			bypass::overlay::text_ex( s_profile_name.c_str( ),
				{ hero_x + 8.0f, avatar_cy + avatar_r + 8.0f, hero_w - 16.0f, 20.0f },
				bypass::overlay::color_t { 248, 246, 251, 240 }, 14.0f,
				bypass::overlay::text_weight_t::black,
				L"Geist-Bold", bypass::overlay::text_align_t::center, false );

			// Level bar
			const float bar_x = hero_x + 16.0f;
			const float bar_y = hero_y + hero_h - 18.0f;
			const float bar_w = hero_w - 32.0f;

			bypass::overlay::filled_rounded_rectangle(
				{ bar_x, bar_y, bar_w, 6.0f }, 3.0f,
				bypass::overlay::color_t { 255, 255, 255, 26 } );

			bypass::overlay::filled_rounded_rectangle(
				{ bar_x, bar_y, ( std::max ) ( 6.0f, bar_w * level_progress ), 6.0f }, 3.0f,
				bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 230 } );

			// Stat chips
			constexpr float chip_h = 46.0f;
			constexpr float chip_gap = 8.0f;
			const float chip_y = hero_y + hero_h + 10.0f;
			const float chip_w = ( hero_w - chip_gap * 2.0f ) / 3.0f;

			wchar_t chip_level[ 16 ] {};
			wchar_t chip_winrate[ 16 ] {};
			wchar_t chip_kd[ 16 ] {};

			std::swprintf( chip_level, std::size( chip_level ), L"%llu",
				static_cast< unsigned long long >( level ) );
			// Zero-value stats read as "noob" instead of 0% / 0.00 so
			// the scoreboard chip still shows something meaningful
			// for a fresh account. Uses the same chip slot / font,
			// so the swap-in never disturbs the row's layout.
			if ( win_rate <= 0.0f )
				std::swprintf( chip_winrate, std::size( chip_winrate ), L"noob" );
			else
				std::swprintf( chip_winrate, std::size( chip_winrate ), L"%.0f%%", win_rate * 100.0f );
			if ( kd <= 0.0f )
				std::swprintf( chip_kd, std::size( chip_kd ), L"noob" );
			else
				std::swprintf( chip_kd, std::size( chip_kd ), L"%.2f", kd );

			struct chip_t { const wchar_t* icon; const wchar_t* label; const wchar_t* value; };
			const chip_t chips[ ] = {
				{ L"\uf005", L"Level",    chip_level },   // star
				{ L"\uf541", L"Win Rate", chip_winrate }, // percent
				{ L"\uf05b", L"K/D",      chip_kd },      // crosshairs
			};

			for ( int i = 0; i < 3; ++i ) {
				const float chip_x = hero_x + static_cast< float >( i ) * ( chip_w + chip_gap );

				bypass::overlay::filled_rounded_rectangle(
					{ chip_x, chip_y, chip_w, chip_h }, 9.0f,
					bypass::overlay::color_t { 22, 19, 35, 190 } );

				bypass::overlay::rounded_rectangle(
					{ chip_x, chip_y, chip_w, chip_h }, 9.0f,
					bypass::overlay::color_t { 70, 66, 92, 70 }, 1.0f );

				bypass::overlay::text_ex( chips[ i ].icon,
					{ chip_x + 5.0f, chip_y, 18.0f, chip_h },
					col_accent, 11.0f, bypass::overlay::text_weight_t::regular,
					L"Font Awesome", bypass::overlay::text_align_t::center, false );

				bypass::overlay::text_ex( chips[ i ].label,
					{ chip_x + 26.0f, chip_y + 6.0f, chip_w - 28.0f, 14.0f },
					bypass::overlay::color_t { 205, 200, 218, 115 }, 9.0f,
					bypass::overlay::text_weight_t::semi_bold,
					L"Inter-Medium", bypass::overlay::text_align_t::left, false );

				bypass::overlay::text_ex( chips[ i ].value,
					{ chip_x + 26.0f, chip_y + 20.0f, chip_w - 28.0f, 20.0f },
					bypass::overlay::color_t { 244, 241, 250, 230 }, 13.0f,
					bypass::overlay::text_weight_t::black,
					L"Geist-Bold", bypass::overlay::text_align_t::left, false );
			}

			// ---- Authoritative lifetime-XP leaderboard -----------------------------
			const float board_hdr_y = chip_y + chip_h + 16.0f;

			wchar_t board_title[ 48 ] {};
			if ( server_profile.placement )
				std::swprintf( board_title, std::size( board_title ), L"SCOREBOARD · #%u / %u",
					server_profile.placement, server_profile.total_players );
			else
				std::swprintf( board_title, std::size( board_title ), L"SCOREBOARD" );
			bypass::overlay::text_ex( board_title,
				{ side_x + 16.0f, board_hdr_y, side_w - 32.0f, 14.0f },
				bypass::overlay::color_t { 205, 200, 218, 115 }, 10.0f,
				bypass::overlay::text_weight_t::semi_bold,
				L"Inter-Medium", bypass::overlay::text_align_t::center, false );

			bypass::overlay::filled_rectangle(
				{ side_x + 16.0f, board_hdr_y + 18.0f, side_w - 32.0f, 1.0f },
				bypass::overlay::color_t { 255, 255, 255, 22 } );

			constexpr float row_h = 26.0f;
			constexpr float row_gap = 5.0f;
			float row_y = board_hdr_y + 26.0f;

			// Row-window selection:
			//   • Player IS placed → show a slice centred on the
			//     player row, equal counts above and below (edges
			//     of the leaderboard shift the window inward when
			//     the player is near rank 1 or the last rank).
			//   • Player is UNPLACED (placement == 0 OR no
			//     current_user row in the array) → show the top-N
			//     rows in natural order (top-to-lowest).
			const std::uint32_t total_rows =
				static_cast< std::uint32_t >( server_profile.leaderboard.size( ) );
			const auto max_rows_f =
				( side_y + side_h - 12.0f - row_y ) / ( row_h + row_gap );
			std::uint32_t max_rows = max_rows_f > 0.f
				? static_cast< std::uint32_t >( max_rows_f )
				: 0u;
			if ( max_rows > total_rows )
				max_rows = total_rows;

			std::uint32_t window_start = 0;
			if ( server_profile.placement && max_rows > 0 && total_rows > max_rows ) {
				std::uint32_t player_idx = total_rows;
				for ( std::uint32_t j = 0; j < total_rows; ++j ) {
					if ( server_profile.leaderboard[ j ].current_user ) {
						player_idx = j;
						break;
					}
				}
				if ( player_idx < total_rows ) {
					const std::uint32_t half_before = max_rows / 2u;
					if ( player_idx <= half_before )
						window_start = 0;
					else if ( player_idx + ( max_rows - half_before ) > total_rows )
						window_start = total_rows - max_rows;
					else
						window_start = player_idx - half_before;
				}
			}

			for ( std::uint32_t offset = 0; offset < max_rows; ++offset ) {
				const std::uint32_t i = window_start + offset;
				if ( i >= total_rows ) break;
				if ( row_y + row_h > side_y + side_h - 12.0f )
					break;

				const auto& row = server_profile.leaderboard[ i ];
				const bypass::overlay::color_t avatar_color = row.current_user ? col_accent
					: bypass::overlay::color_t { static_cast< std::uint8_t >( 70 + row.placement * 23 % 150 ),
					static_cast< std::uint8_t >( 80 + row.placement * 37 % 140 ),
					static_cast< std::uint8_t >( 100 + row.placement * 17 % 130 ), 255 };

				bypass::overlay::filled_rounded_rectangle(
					{ side_x + 12.0f, row_y, side_w - 24.0f, row_h }, 7.0f,
					row.current_user
					? bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 34 }
				: bypass::overlay::color_t { 22, 19, 35, 160 } );

				bypass::overlay::rounded_rectangle(
					{ side_x + 12.0f, row_y, side_w - 24.0f, row_h }, 7.0f,
					row.current_user
					? bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 100 }
				: bypass::overlay::color_t { 255, 255, 255, 16 }, 1.0f );

				wchar_t rank_text[ 8 ] {};
				std::swprintf( rank_text, std::size( rank_text ), L"#%u", row.placement );

				bypass::overlay::text_ex( rank_text,
					{ side_x + 20.0f, row_y, 24.0f, row_h },
					row.current_user ? col_text_active : bypass::overlay::color_t { 205, 200, 218, 130 },
					10.0f, bypass::overlay::text_weight_t::semi_bold,
					L"Inter-Medium", bypass::overlay::text_align_t::left, false );

				const float av_x = side_x + 48.0f;
				bypass::overlay::filled_circle( { av_x + 7.0f, row_y + row_h * 0.5f }, 7.0f,
					avatar_color );

				const wchar_t initial[ 2 ] = { row.display_name.empty( ) ? L'?' : row.display_name[ 0 ], L'\0' };
				bypass::overlay::text_ex( initial,
					{ av_x, row_y, 14.0f, row_h },
					bypass::overlay::color_t { 255, 255, 255, 235 }, 8.0f,
					bypass::overlay::text_weight_t::bold,
					L"Geist-Bold", bypass::overlay::text_align_t::center, false );

				bypass::overlay::text_ex( row.display_name.c_str( ),
					{ side_x + 70.0f, row_y, 110.0f, row_h },
					row.current_user
					? bypass::overlay::color_t { 248, 246, 251, 240 }
					: bypass::overlay::color_t { 226, 222, 236, 190 },
					11.0f, bypass::overlay::text_weight_t::semi_bold,
					L"Geist-Medium", bypass::overlay::text_align_t::left, false );

				wchar_t score_text[ 32 ] {};
				std::swprintf( score_text, std::size( score_text ), L"Lv.%u · %.1fK", row.level,
					static_cast< double >( row.lifetime_xp ) / 1000.0 );

				bypass::overlay::text_ex( score_text,
					{ side_x + 12.0f, row_y, side_w - 32.0f, row_h },
					row.current_user ? col_text_active : bypass::overlay::color_t { 205, 200, 218, 140 },
					10.5f, bypass::overlay::text_weight_t::semi_bold,
					L"Inter-Medium", bypass::overlay::text_align_t::right, false );

				row_y += row_h + row_gap;
			}
		}
	}

	void render( bool menu_open ) {

		{
			static bool s_icons_patched = false;
			if ( !s_icons_patched ) {
				static const wchar_t s_weapon_icon   [ 2 ] = { L'\xE54E', L'\0' };
				static const wchar_t s_world_icon    [ 2 ] = { L'\xE547', L'\0' };
				static const wchar_t s_snaplines_icon[ 2 ] = { L'\xF124', L'\0' };
				visuals_info_children[ 1 ].icon = s_weapon_icon;
				visuals_info_children[ 2 ].icon = s_world_icon;
				visuals_children     [ 2 ].icon = s_snaplines_icon;
				s_icons_patched = true;
			}
		}

		// ---- Input (always needed) -------------------------------------------
		POINT cursor {};
		GetCursorPos( &cursor );
		const float mx = static_cast< float >( cursor.x );
		const float my = static_cast< float >( cursor.y );
		const bool  lmb = ( GetAsyncKeyState( VK_LBUTTON ) & 0x8000 ) != 0;
		const bool  clicked = lmb && !s_lmb_prev;
		const bool  rmb = ( GetAsyncKeyState( VK_RBUTTON ) & 0x8000 ) != 0;
		const bool  right_clicked = rmb && !s_rmb_prev;
		s_lmb_prev = lmb;
		s_rmb_prev = rmb;
		update_search_input( );

		const auto animation_now = std::chrono::steady_clock::now( );
		float animation_delta = std::chrono::duration<float>( animation_now - s_last_animation_tick ).count( );

		// Fallback to ~60 FPS if the clock hiccups or returns 0
		if ( animation_delta <= 0.0f || animation_delta > 0.1f )
			animation_delta = 0.016f;

		s_last_animation_tick = animation_now;
		s_last_animation_delta = animation_delta;
		const float animation_ease = 1.0f - std::exp( -animation_delta * 12.0f );

		// ---- Palette (always needed for watermark + notifications) ------------
		constexpr bypass::overlay::color_t col_bg { 10, 9, 18, 255 };
		constexpr bypass::overlay::color_t col_nav_overlay { 22, 19, 35, 90 };
		constexpr bypass::overlay::color_t col_right_overlay { 18, 15, 27, 200 };
		const bypass::overlay::color_t col_accent = current_accent( );
		const bypass::overlay::color_t col_accent_bright {
			static_cast< std::uint8_t >( ( std::min ) ( 255u, col_accent.r + 20u ) ),
			static_cast< std::uint8_t >( ( std::min ) ( 255u, col_accent.g + 30u ) ),
			static_cast< std::uint8_t >( ( std::min ) ( 255u, col_accent.b + 20u ) ), 240 };
		constexpr bypass::overlay::color_t col_border { 55, 55, 55, 220 };
		const bypass::overlay::color_t col_text_active = col_accent_bright;

		// ================================================================
		//  PANEL — only drawn when the menu is open
		// ================================================================
		if ( menu_open ) {
			if ( s_show_background ) {
				const float sw = static_cast< float >( gui::screen_width );
				const float sh = static_cast< float >( gui::screen_height );

				// Base backdrop is 80 alpha; widget-layout mode
				// deepens it by ~25 alpha so the screen reads as
				// clearly ~20-25% darker, drawing focus onto the
				// widgets you're arranging on top.
				const std::uint8_t base_dim_a = s_widget_layout_mode ? 105u : 80u;
				bypass::overlay::filled_rectangle(
					{ 0.f, 0.f, sw, sh },
					bypass::overlay::color_t { 0, 0, 0, base_dim_a }
				);

				static float s_aura_intensity = 0.f;
				s_aura_intensity = min( 1.f, s_aura_intensity + animation_delta * 2.f );

				// Vertical gradient: transparent at ~60 % down → soft accent at
				// the bottom edge.  Capped at 110 alpha so it never blows out.
				const std::uint8_t eff_a = static_cast< std::uint8_t >( 110.f * s_aura_intensity );
				bypass::overlay::gradient_rectangle(
					{ 0.f, sh * 0.58f, sw, sh * 0.42f },
					{ col_accent.r, col_accent.g, col_accent.b, 0 },   // transparent at top
					{ col_accent.r, col_accent.g, col_accent.b, eff_a },   // soft accent at bottom
					0.f
				);
			}

			// Animation progress
			const float sidebar_target = s_main_tab == 0 ? 1.0f : 0.0f;
			s_feature_sidebar_progress += ( sidebar_target - s_feature_sidebar_progress ) * animation_ease;
			for ( std::uint32_t i = 0; i < static_cast< std::uint32_t >( s_dropdown_progress.size( ) ); ++i ) {
				const float target = ( s_expanded_tabs & ( 1u << i ) ) != 0 ? 1.0f : 0.0f;
				s_dropdown_progress[ i ] += ( target - s_dropdown_progress[ i ] ) * animation_ease;
				if ( std::abs( target - s_dropdown_progress[ i ] ) < 0.002f )
					s_dropdown_progress[ i ] = target;
			}
			s_tab_content_progress += ( 1.0f - s_tab_content_progress ) * animation_ease;
			if ( std::abs( sidebar_target - s_feature_sidebar_progress ) < 0.002f )
				s_feature_sidebar_progress = sidebar_target;
			if ( 1.0f - s_tab_content_progress < 0.002f )
				s_tab_content_progress = 1.0f;

			// Drag
			if ( s_pos_x < 0.0f ) {
				s_pos_x = ( static_cast< float >( gui::screen_width ) - panel_w ) * 0.5f
					+ ( nav_rail_w + nav_rail_gap ) * 0.5f;
				s_pos_y = ( static_cast< float >( gui::screen_height ) - panel_h ) * 0.5f;
			}
			const float drag_zone_h = pad_y + header_h;
			if ( clicked && hit_test( mx, my, s_pos_x, s_pos_y, panel_w, drag_zone_h ) ) {
				s_dragging = true;
				s_drag_off_x = mx - s_pos_x;
				s_drag_off_y = my - s_pos_y;
			}
			if ( !lmb )       s_dragging = false;
			if ( s_dragging ) { s_pos_x = mx - s_drag_off_x; s_pos_y = my - s_drag_off_y; }

			const float shake_now = std::chrono::duration< float >( std::chrono::steady_clock::now( ).time_since_epoch( ) ).count( );
			const float shake_age = shake_now - s_shake_at;
			const float shake_amp = ( shake_age >= 0.0f && shake_age < 0.55f ) ? std::sin( shake_age * 62.0f ) * ( 1.0f - shake_age / 0.55f ) * 7.0f : 0.0f;
			const float px = s_pos_x + shake_amp;
			const float py = s_pos_y + shake_amp * 0.5f;
			const bypass::overlay::rect_t panel { px, py, panel_w, panel_h };

			// Assets — the overlay's tinted image path uses FillOpacityMask
			// which reads only the bitmap's alpha channel, so tinting the
			// logo to the accent colour needs no bitmap preprocessing here.
			if ( !s_logo_registered )
				s_logo_registered = bypass::overlay::register_image(
					logo_id, L"C:\\crisis\\logo.png" );

			// 1. Base
			const bool home_layout = s_main_tab == 0;
			const float effective_sidebar_w = home_layout ? sidebar_w : 0.0f;
			bypass::overlay::filled_rounded_rectangle( panel, panel_radius, col_bg );
			if ( home_layout ) {
				bypass::overlay::filled_rounded_rectangle(
					{ px, py, effective_sidebar_w + panel_radius, panel_h },
					panel_radius, col_nav_overlay );
				bypass::overlay::filled_rectangle(
					{ px + effective_sidebar_w, py, panel_w - effective_sidebar_w - panel_radius, panel_h },
					col_bg );
				bypass::overlay::filled_rounded_rectangle(
					{ px + effective_sidebar_w, py, panel_w - effective_sidebar_w, panel_h },
					panel_radius, col_right_overlay );
			}
			else {
				bypass::overlay::filled_rounded_rectangle( panel, panel_radius, col_right_overlay );
			}

			// Nav rail
			const float nav_rail_x = px - nav_rail_gap - nav_rail_w;
			const float nav_rail_y = py + ( panel_h - nav_rail_h ) * 0.5f;
			bypass::overlay::filled_rounded_rectangle(
				{ nav_rail_x, nav_rail_y, nav_rail_w, nav_rail_h },
				16.0f, bypass::overlay::color_t { 18, 16, 31, 245 } );
			bypass::overlay::rounded_rectangle(
				{ nav_rail_x, nav_rail_y, nav_rail_w, nav_rail_h },
				16.0f, bypass::overlay::color_t { 62, 66, 84, 135 }, 1.0f );

			// Vertically centre the button GROUP (main tabs + the
			// widget-layout icon below) inside the rail. Computed
			// from nav_rail_button_count so a future button just
			// bumps the constant and everything re-centres.
			const float nav_rail_content_h =
				( nav_rail_button_count - 1.0f ) * nav_rail_item_spacing
				+ nav_rail_item_size;
			const float nav_rail_start_y =
				nav_rail_y + ( nav_rail_h - nav_rail_content_h ) * 0.5f;

			std::int32_t hovered_main_tab = -1;
			for ( std::uint32_t i = 0; i < static_cast< std::uint32_t >( std::size( main_tabs ) ); ++i ) {
				const float item_x = nav_rail_x + ( nav_rail_w - nav_rail_item_size ) * 0.5f;
				const float item_y = nav_rail_start_y + i * nav_rail_item_spacing;
				const bool hovered = hit_test( mx, my, item_x, item_y, 36.0f, 36.0f );
				const bool active = i == s_main_tab;
				if ( hovered ) hovered_main_tab = static_cast< std::int32_t >( i );
				if ( clicked && hovered ) s_main_tab = i;
				if ( active || hovered ) {
					bypass::overlay::filled_rounded_rectangle(
						{ item_x, item_y, 36.0f, 36.0f }, 10.0f,
						active ? bypass::overlay::color_t { 38, 36, 52, 90 }
					: bypass::overlay::color_t { 255, 255, 255, 10 } );
					if ( active )
						bypass::overlay::rounded_rectangle(
							{ item_x, item_y, 36.0f, 36.0f }, 10.0f,
							bypass::overlay::color_t { 60, 58, 76, 160 }, 1.0f );
				}
				bypass::overlay::text_ex(
					main_tabs[ i ].icon,
					{ item_x, item_y, 36.0f, 36.0f },
					active ? col_accent_bright
					: bypass::overlay::color_t { 220, 216, 232, static_cast< std::uint8_t >( hovered ? 190u : 105u ) },
					14.0f, bypass::overlay::text_weight_t::regular,
					L"Font Awesome", bypass::overlay::text_align_t::center, false );
			}

			// ── Widget Layout toggle (DEPRECATED, disabled) ────
			// Rail is centred on 3 main tabs; the block below is
			// gated to `if (false)` so it no longer renders or
			// hit-tests. The widget_layout::* mode + overlay
			// framework are still intact and can be reactivated
			// later by flipping this gate back on and bumping
			// nav_rail_button_count to 4.
			if ( false )
			// ── Widget Layout toggle (icon-only, no dropdown) ──
			// Sits directly below the last main tab in the global
			// nav rail. Reuses the exact same 36x36 pill geometry
			// so it visually belongs to the tab family, but flips
			// s_widget_layout_mode on click instead of swapping the
			// active main tab. Icon is Font Awesome arrows-alt
			// () per the spec.
			{
				const float item_x = nav_rail_x + ( nav_rail_w - nav_rail_item_size ) * 0.5f;
				// Slots directly after the last main tab, sharing
				// the same centred group start_y so all four buttons
				// line up on identical spacing.
				const float item_y = nav_rail_start_y
					+ static_cast< float >( std::size( main_tabs ) ) * nav_rail_item_spacing;
				const bool  wlm_hovered = hit_test( mx, my, item_x, item_y, nav_rail_item_size, nav_rail_item_size );
				const bool  wlm_active  = s_widget_layout_mode;
				if ( clicked && wlm_hovered )
					s_widget_layout_mode = !s_widget_layout_mode;

				if ( wlm_active || wlm_hovered ) {
					bypass::overlay::filled_rounded_rectangle(
						{ item_x, item_y, nav_rail_item_size, nav_rail_item_size }, 10.0f,
						wlm_active
						? bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 60u }
						: bypass::overlay::color_t { 255, 255, 255, 10u } );
					if ( wlm_active )
						bypass::overlay::rounded_rectangle(
							{ item_x, item_y, nav_rail_item_size, nav_rail_item_size }, 10.0f,
							bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 180u }, 1.0f );
				}

				bypass::overlay::text_ex( L"",
					{ item_x, item_y, nav_rail_item_size, nav_rail_item_size },
					wlm_active ? col_accent_bright
					: bypass::overlay::color_t { 220, 216, 232, static_cast< std::uint8_t >( wlm_hovered ? 190u : 105u ) },
					14.0f, bypass::overlay::text_weight_t::regular,
					L"Font Awesome", bypass::overlay::text_align_t::center, false );
			}

			// Header / Logo
			const float header_top = py + pad_y;
			const float header_cy = header_top + header_h * 0.5f;
			constexpr float logo_h = 30.0f;
			constexpr float logo_w = 158.0f;
			if ( s_feature_sidebar_progress > 0.01f && s_logo_registered )
				// The logo bitmap was registered with mask=true so its RGB
				// channels are pre-stripped to premultiplied white. Tinting
				// with col_accent here renders the shape in a single flat
				// accent colour — no source-image bleed possible.
				bypass::overlay::image( logo_id,
					{ px + 17.0f, py + 18.0f, logo_w, logo_h },
					col_accent, s_feature_sidebar_progress, 0.0f );
			if ( s_feature_sidebar_progress > 0.01f ) {
				constexpr int logo_divider_segments = 64;
				constexpr float logo_divider_w = 158.0f;
				const float logo_divider_x = px + 17.0f;
				const float logo_divider_y = py + 59.0f;
				for ( int i = 0; i < logo_divider_segments; ++i ) {
					const float x0 = logo_divider_x + logo_divider_w * ( float ) i / logo_divider_segments;
					const float x1 = logo_divider_x + logo_divider_w * ( float ) ( i + 1 ) / logo_divider_segments;
					const float position = ( ( float ) i + 0.5f ) / logo_divider_segments;
					float fade = ( std::min ) ( position, 1.0f - position ) / 0.38f;
					fade = ( std::clamp ) ( fade, 0.0f, 1.0f );
					fade = fade * fade * ( 3.0f - 2.0f * fade );
					const auto divider_alpha = static_cast< std::uint8_t >(
						90.0f * fade * s_feature_sidebar_progress );
					if ( divider_alpha > 0 )
						// Track col_accent so the divider matches the logo
						// tint above it across every theme colour.
						bypass::overlay::filled_rectangle( { x0, logo_divider_y, x1 - x0, 2.0f },
							bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, divider_alpha } );
				}
			}

			// 4. Sidebar
			const float     sidebar_x = px;
			const float     separator_y = header_top + header_h + 12.0f;
			const float     sidebar_y = separator_y + 1.0f;
			const float     sidebar_h = ( py + panel_h ) - sidebar_y - panel_radius;

			constexpr float sb_pad_x = 16.0f;
			constexpr float sb_item_h = 32.0f;
			constexpr float sb_sub_h = 30.0f;
			constexpr float sb_icon_w = 16.0f;
			constexpr float sb_icon_g = 8.0f;
			constexpr float sb_indent = 22.0f;
			constexpr float sb_item_gap = 6.0f;

			float item_y = sidebar_y - 2.0f;

			if ( s_feature_sidebar_progress > 0.01f ) {
				auto draw_tab_outline = [ & ]( float x, float y, float w, float h, float r,
					bypass::overlay::color_t col, float peak_alpha, float base_alpha, float max_thick )
					{
						// whisper-faint full outline so the tab edge still reads
						bypass::overlay::rounded_rectangle( { x, y, w, h }, r,
							bypass::overlay::color_t { col.r, col.g, col.b, static_cast< std::uint8_t >( base_alpha ) }, 1.0f );

						// bright stroke concentrated at the top-right, fading at both ends
						constexpr int segs = 48;
						const float ccx = x + w - r;   // top-right arc centre
						const float ccy = y + r;

						bypass::overlay::point_t prev { x + w * 0.5f, y };
						for ( int i = 1; i <= segs; ++i ) {
							const float s = static_cast< float >( i ) / segs;
							bypass::overlay::point_t p;
							if ( s < 0.45f ) {                                   // top edge, right half
								const float t = s / 0.45f;
								p = { x + w * 0.5f + ( w * 0.5f - r ) * t, y };
							}
							else if ( s < 0.75f ) {                              // top-right corner arc
								const float t = ( s - 0.45f ) / 0.30f;
								const float a = -1.5707963f * ( 1.0f - t );    // -90° → 0°
								p = { ccx + std::cos( a ) * r, ccy + std::sin( a ) * r };
							}
							else {                                               // right edge, top half
								const float t = ( s - 0.75f ) / 0.25f;
								p = { x + w, y + r + ( h * 0.5f - r ) * t };
							}

							// 0 at both ends, 1 right on the corner → the "transition out"
							const float fade = std::sin( s * 3.14159265f );
							const auto a8 = static_cast< std::uint8_t >( peak_alpha * fade );

							if ( a8 > 0 ) {
								// FIX 1: Use a constant thickness. Varying the thickness per-segment 
								// causes the quads to have different widths, creating overlaps and 
								// gaps at the joints that look like "broken dots" or beads.
								const float thick = max_thick;

								bypass::overlay::line( prev, p,
									bypass::overlay::color_t { col.r, col.g, col.b, a8 }, thick );

								// FIX 2: Basic line renderers often leave gaps at the inner corners 
								// of polyline joints (no miter joins). Drawing a small filled circle 
								// at each vertex fills these gaps and creates a perfectly smooth curve.
								bypass::overlay::filled_circle( p, thick * 0.5f,
									bypass::overlay::color_t { col.r, col.g, col.b, a8 } );
							}
							prev = p;
						}
					};
				for ( std::uint32_t i = 0; i < static_cast< std::uint32_t >( std::size( sidebar_tabs ) ); ++i ) {
					const sidebar_tab_t& st = sidebar_tabs[ i ];
					const bool           active = ( i == s_sidebar_tab );
					const bool           expanded = ( st.sub_count > 0 ) && ( s_expanded_tabs & ( 1u << i ) ) != 0;
					const float          dropdown_progress = s_dropdown_progress[ i ];
					const bool           visual_expanded = expanded || dropdown_progress > 0.01f;

					const bool hdr_hovered = hit_test( mx, my,
						sidebar_x + 8.0f, item_y, sidebar_w - 16.0f, sb_item_h );

					if ( clicked && hit_test( mx, my, sidebar_x, item_y, sidebar_w, sb_item_h ) ) {
						const bool selecting_new_tab = i != s_sidebar_tab;
						s_sidebar_tab = i;
						s_sidebar_subtab = 0;
						if ( st.sub_count > 0 ) {
							if ( selecting_new_tab )
								s_expanded_tabs |= 1u << i;
							else
								s_expanded_tabs ^= 1u << i;
						}
					}
					bypass::overlay::filled_rounded_rectangle(
						{ sidebar_x + 8.0f, item_y, sidebar_w - 16.0f, sb_item_h }, 8.0f,
						bypass::overlay::color_t { 26, 24, 39, static_cast< std::uint8_t >( visual_expanded ? 220u : 135u ) } );
					bypass::overlay::rounded_rectangle(
						{ sidebar_x + 8.0f, item_y, sidebar_w - 16.0f, sb_item_h }, 8.0f,
						bypass::overlay::color_t { 72, 68, 86, static_cast< std::uint8_t >( visual_expanded ? 105u : 55u ) }, 1.0f );

					// ── Section header row — muted, like the reference group labels ──
					if ( hdr_hovered && !visual_expanded ) {
						bypass::overlay::filled_rounded_rectangle(
							{ sidebar_x + 10.0f, item_y, sidebar_w - 20.0f, sb_item_h },
							8.0f, bypass::overlay::color_t { 255, 255, 255, 6 } );
					}

					const auto icon_col = visual_expanded
						? bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 210 }
						: hdr_hovered
						? bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 190 }
					: bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 130 };
					bypass::overlay::text_ex( st.icon,
						{ sidebar_x + sb_pad_x + 2.0f, item_y, sb_icon_w, sb_item_h },
						icon_col, 12.0f, bypass::overlay::text_weight_t::regular,
						L"Font Awesome", bypass::overlay::text_align_t::center, false );

					const auto lbl_col = hdr_hovered
						? bypass::overlay::color_t { 255, 255, 255, 210 }
					: bypass::overlay::color_t { 200, 198, 214, static_cast< std::uint8_t >( visual_expanded ? 165u : 130u ) };
					bypass::overlay::text_ex( st.label,
						{ sidebar_x + sb_pad_x + sb_icon_w + sb_icon_g + 2.0f, item_y,
						  sidebar_w - sb_pad_x - sb_icon_w - sb_icon_g - 20.0f, sb_item_h },
						lbl_col, 12.5f,
						bypass::overlay::text_weight_t::semi_bold,
						L"Geist-Medium", bypass::overlay::text_align_t::left, false );

					// Collapsed-but-active badge (the tiny square from the reference)
					if ( active && !visual_expanded ) {
						const float bx = sidebar_x + sidebar_w - sb_pad_x - 14.0f;
						const float by = item_y + ( sb_item_h - 14.0f ) * 0.5f;
						bypass::overlay::filled_rounded_rectangle(
							{ bx, by, 14.0f, 14.0f }, 4.0f,
							bypass::overlay::color_t { 255, 255, 255, 12 } );
						bypass::overlay::filled_rounded_rectangle(
							{ bx + 5.0f, by + 5.0f, 4.0f, 4.0f }, 1.0f,
							bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 235 } );
					}

					if ( st.sub_count > 0 ) {
						bypass::overlay::text_ex(
							expanded ? L"\uf077" : L"\uf078",
							{ sidebar_x + sidebar_w - sb_pad_x - 14.0f, item_y, 14.0f, sb_item_h },
							bypass::overlay::color_t { 198, 195, 213, static_cast< std::uint8_t >( expanded ? 150u : 70u ) }, 9.0f,
							bypass::overlay::text_weight_t::regular, L"Font Awesome",
							bypass::overlay::text_align_t::center, false );
					}

					item_y += sb_item_h + 2.0f;

					// ── Dropdown items — reference-style list ──
					if ( dropdown_progress > 0.01f ) {
						const float animated_sub_h = sb_sub_h * dropdown_progress;
						const float dropdown_y = item_y;

						for ( std::uint32_t j = 0; j < st.sub_count; ++j ) {
							const float sub_y = dropdown_y + j * animated_sub_h;
							const bool sub_active = active && ( j == s_sidebar_subtab );
							const bool sub_hovered = hit_test( mx, my,
								sidebar_x + sb_pad_x + 14.0f, sub_y,
								sidebar_w - sb_pad_x - 22.0f, animated_sub_h );

							if ( dropdown_progress > 0.92f && clicked
								&& hit_test( mx, my, sidebar_x, sub_y, sidebar_w, animated_sub_h ) ) {
								s_main_tab = 0;
								s_sidebar_tab = i;
								s_sidebar_subtab = j;
							}

							const float row_x = sidebar_x + 8.0f;
							const float row_w = sidebar_w - 16.0f;
							const float row_y = sub_y + 1.0f;
							const float row_h = animated_sub_h - 2.0f;
							if ( sub_active ) {
								const float row_x = sidebar_x + 10.0f;
								const float row_y = sub_y;
								const float row_w = sidebar_w - 20.0f;
								const float row_h = animated_sub_h;

								// Elevated pill (neutral translucent background, NOT purple)
								bypass::overlay::filled_rounded_rectangle(
									{ row_x, row_y, row_w, row_h }, 8.0f,
									bypass::overlay::color_t { 255, 255, 255, 12 } );
								bypass::overlay::rounded_rectangle(
									{ row_x, row_y, row_w, row_h }, 8.0f,
									bypass::overlay::color_t { 255, 255, 255, 5 }, 1.0f );

								// Glass top half + corner sheen, clipped to the pill
								bypass::overlay::push_clip( { row_x, row_y, row_w, row_h } );
								bypass::overlay::filled_rounded_rectangle(
									{ row_x, row_y, row_w, row_h * 0.5f }, 7.0f,
									bypass::overlay::color_t { 255, 255, 255, 7 } );
								bypass::overlay::filled_circle(
									{ row_x + row_w - 6.0f, row_y + 2.0f }, 26.0f,
									bypass::overlay::color_t { 255, 255, 255, 9 } );
								bypass::overlay::filled_circle(
									{ row_x + row_w - 2.0f, row_y - 2.0f }, 14.0f,
									bypass::overlay::color_t { 255, 255, 255, 8 } );
								bypass::overlay::pop_clip( );


								// Right-side purple indicator bar
								bypass::overlay::filled_rounded_rectangle(
									{ row_x + row_w - 4.0f, row_y + row_h * 0.5f - 7.0f, 3.0f, 14.0f },
									1.5f,
									bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b,
										static_cast< std::uint8_t >( 255.0f * dropdown_progress ) } );
							}
							else if ( sub_hovered ) {
								bypass::overlay::filled_rounded_rectangle(
									{ row_x, row_y, row_w, row_h }, 10.0f,
									bypass::overlay::color_t { 255, 255, 255, 5 } );
							}

							const auto sub_col = sub_active
								? bypass::overlay::color_t { 250, 249, 255,
									static_cast< std::uint8_t >( 240.0f * dropdown_progress ) }
								: sub_hovered
								? bypass::overlay::color_t { 255, 255, 255,
									static_cast< std::uint8_t >( 200.0f * dropdown_progress ) }
								: bypass::overlay::color_t { 176, 173, 192,
									static_cast< std::uint8_t >( 120.0f * dropdown_progress ) };
							bypass::overlay::text_ex( st.subs[ j ].label,
								{ sidebar_x + sb_pad_x + sb_indent, sub_y,
								  sidebar_w - sb_pad_x - sb_indent - 12.0f, animated_sub_h },
								sub_col, sub_active ? 13.0f : 12.5f,
								sub_active ? bypass::overlay::text_weight_t::semi_bold : bypass::overlay::text_weight_t::medium,
								L"Geist-Medium", bypass::overlay::text_align_t::left, false );
						}
						item_y += st.sub_count * animated_sub_h;
					}
					item_y += sb_item_gap;
				}

				// ── Widget Layout Mode button (LEGACY, now hidden) ──
				// The toggle moved to the global (main-tab) nav rail
				// as an icon-only button so it's reachable from every
				// page, not just Home. The block below is gated to
				// never render — kept for one commit so a git diff
				// can show the move rather than a disappear.
				if ( false ) {
					const bool wlm_hover  = hit_test( mx, my, sidebar_x, item_y, sidebar_w, sb_item_h );
					const bool wlm_active = s_widget_layout_mode;
					if ( clicked && wlm_hover )
						s_widget_layout_mode = !s_widget_layout_mode;

					// Background pill — matches tab item styling
					// so the button reads as belonging to the
					// same sidebar family. Active state uses the
					// accent-tinted fill to signal the mode.
					const auto pill_fill = wlm_active
						? bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 60u }
						: bypass::overlay::color_t { 26, 24, 39, static_cast< std::uint8_t >( wlm_hover ? 220u : 135u ) };
					const auto pill_border = wlm_active
						? bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 180u }
						: bypass::overlay::color_t { 72, 68, 86, static_cast< std::uint8_t >( wlm_hover ? 105u : 55u ) };
					bypass::overlay::filled_rounded_rectangle(
						{ sidebar_x + 8.0f, item_y, sidebar_w - 16.0f, sb_item_h }, 8.0f,
						pill_fill );
					bypass::overlay::rounded_rectangle(
						{ sidebar_x + 8.0f, item_y, sidebar_w - 16.0f, sb_item_h }, 8.0f,
						pill_border, 1.0f );

					// Icon — Font Awesome arrows-alt ()
					// per the spec. Uses accent tint when active
					// to match the pill fill.
					const auto icon_col_wlm = wlm_active
						? bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 235u }
						: wlm_hover
						? bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 190u }
						: bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 130u };
					bypass::overlay::text_ex( L"",
						{ sidebar_x + sb_pad_x + 2.0f, item_y, sb_icon_w, sb_item_h },
						icon_col_wlm, 12.0f, bypass::overlay::text_weight_t::regular,
						L"Font Awesome", bypass::overlay::text_align_t::center, false );

					// Label
					const auto lbl_col_wlm = wlm_active
						? bypass::overlay::color_t { 255, 255, 255, 235u }
						: wlm_hover
						? bypass::overlay::color_t { 255, 255, 255, 210u }
						: bypass::overlay::color_t { 200, 198, 214, 130u };
					bypass::overlay::text_ex( L"Widget Layout",
						{ sidebar_x + sb_pad_x + sb_icon_w + sb_icon_g + 2.0f, item_y,
						  sidebar_w - sb_pad_x - sb_icon_w - sb_icon_g - 20.0f, sb_item_h },
						lbl_col_wlm, 12.5f,
						bypass::overlay::text_weight_t::semi_bold,
						L"Geist-Medium", bypass::overlay::text_align_t::left, false );

					item_y += sb_item_h + sb_item_gap;
				}

				struct social_icon_t {
					const wchar_t* icon;
					const wchar_t* font;
				};
				static constexpr social_icon_t social_icons[ ] = {
					{ L"", L"Font Awesome Brands" },
					{ L"", L"Font Awesome Brands" },
					{ L"", L"Font Awesome" },
				};
				constexpr float social_item_sz = 32.0f;
				constexpr float social_gap = 8.0f;
				const float social_row_w = social_item_sz * 3.0f + social_gap * 2.0f;
				const float social_x = sidebar_x + ( sidebar_w - social_row_w ) * 0.5f;
				const float social_y = py + panel_h - panel_radius - social_item_sz + 6.0f;

				for ( std::uint32_t i = 0; i < static_cast< std::uint32_t >( std::size( social_icons ) ); ++i ) {
					const float icon_x = social_x + i * ( social_item_sz + social_gap );
					const bool hovered = hit_test( mx, my, icon_x, social_y, social_item_sz, social_item_sz );

					if ( hovered )
						bypass::overlay::filled_rounded_rectangle(
							{ icon_x, social_y, social_item_sz, social_item_sz }, 7.0f,
							bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 25 } );

					bypass::overlay::text_ex(
						social_icons[ i ].icon,
						{ icon_x, social_y, social_item_sz, social_item_sz },
						hovered
						? col_accent_bright
						: bypass::overlay::color_t { 214, 210, 225, 105 },
						15.0f, bypass::overlay::text_weight_t::regular,
						social_icons[ i ].font, bypass::overlay::text_align_t::center, false );
				}
				if ( s_feature_sidebar_progress < 1.0f )
					bypass::overlay::filled_rectangle(
						{ px, sidebar_y, sidebar_w, sidebar_h },
						bypass::overlay::color_t { 15, 13, 26,
							static_cast< std::uint8_t >( ( 1.0f - s_feature_sidebar_progress ) * 255.0f ) } );
			}

			// Content pane
			const std::uint32_t content_key = ( s_main_tab << 16 ) | ( s_sidebar_tab << 8 ) | s_sidebar_subtab;
			if ( s_animated_content_key != content_key ) {
				s_animated_content_key = content_key;
				s_tab_content_progress = 0.0f;
			}
			const float content_x = home_layout ? px + sidebar_w + 1.0f : px + 1.0f;
			const float content_y = py + 2.0f;
			const float content_w = home_layout ? panel_w - sidebar_w - 1.0f : panel_w - 2.0f;
			if ( home_layout )
				bypass::overlay::filled_rectangle(
					{ px + sidebar_w, py, 1.0f, panel_h }, col_border );

			s_dropdown_draw_queue.clear( );
			if ( s_main_tab == 0 )
				render_content( content_x, content_y, content_w, child_layout_h,
					mx, my, lmb, clicked, right_clicked, col_accent, col_border, col_text_active );
			else
				render_main_tab_content( content_x, content_y, content_w,
					mx, my, lmb, clicked, col_accent, col_border, col_text_active );

			// Visuals-tab side element — a persistent 3D preview card
			// floated to the RIGHT of the main window, only visible
			// while the Visuals sub-tab is active. Mirrors the profile
			// page's right-side card placement. Gives the 3D model its
			// own dedicated space so it no longer competes with Boxes /
			// Skeleton for width inside the main panel (which was the
			// source of the "toes + top of head clipping" problem —
			// there was no vertical room to pull the camera back
			// without shrinking the on-screen model to nothing).
			if ( s_main_tab == 0 && s_sidebar_tab == 1 ) {
				// State — persistent across frames. Keeps the toggle
				// value AND its smoothed animation progress here so
				// the block is self-contained.
				static bool  s_esp_expanded          = true;
				// When false, the box + health/shield bar + text stack
				// on the character preview are skipped so only the raw
				// 3D model is visible. Local to the preview — does not
				// touch the in-game ESP toggles.
				static bool  s_esp_preview_show_esp  = true;
				static float s_esp_anim     = 0.0f;   // 0 = fully expanded, 1 = fully minimised

				const float esp_target = s_esp_expanded ? 0.0f : 1.0f;
				s_esp_anim += ( esp_target - s_esp_anim ) * 0.18f;
				if ( std::fabs( esp_target - s_esp_anim ) < 0.001f )
					s_esp_anim = esp_target;
				// Smoothstep-ease so width motion + content fade feel weighted.
				const float t_eased = s_esp_anim * s_esp_anim * ( 3.0f - 2.0f * s_esp_anim );

				constexpr float full_side_w = 312.0f;   // was 480 — dropped 35 % per request
				constexpr float min_side_w  = 34.0f;    // just wider than the toggle box + insets
				// Height also animates now (2026-08-26). When
				// minimised the card shrinks to just a button + a
				// couple of pixels of padding, matching the width
				// collapse. The eye button then lives OUTSIDE the
				// card (to its left) and a vertical divider fades in
				// between them — see the button block below.
				constexpr float min_side_h  = 34.0f;    // btn_size 24 + 2×5 padding
				const float side_w = full_side_w + ( min_side_w - full_side_w ) * t_eased;
				const float side_h = panel_h + ( min_side_h - panel_h ) * t_eased;
				// Anchor the LEFT edge at the same X the expanded card
				// would sit at — as the card shrinks, its right edge
				// pulls leftward and the strip stays attached to the
				// menu's right side (the "closest side").
				const float side_x = ( std::min )(
					content_x + content_w + 6.0f,
					static_cast< float >( gui::screen_width ) - full_side_w - 12.0f );
				const float side_y = py;

				// Content alpha — biased so content clears in the
				// FIRST half of the animation, leaving the last half
				// as a clean strip collapse. Same fade drives header
				// text, accent divider, 3D model opacity.
				float content_alpha_f = 1.0f - ( ( t_eased - 0.05f ) / 0.50f );
				content_alpha_f = ( std::clamp )( content_alpha_f, 0.0f, 1.0f );

				// Backing card — same layered styling as the profile side.
				bypass::overlay::filled_rounded_rectangle(
					{ side_x, side_y, side_w, side_h }, panel_radius,
					bypass::overlay::color_t { 15, 13, 26, 255 } );
				bypass::overlay::filled_rounded_rectangle(
					{ side_x, side_y, side_w, side_h }, panel_radius,
					bypass::overlay::color_t { 32, 29, 42, 44 } );
				bypass::overlay::rounded_rectangle(
					{ side_x, side_y, side_w, side_h }, panel_radius,
					bypass::overlay::color_t { 62, 66, 84, 135 }, 1.0f );

				#if 0  /* Header + accent divider removed per user request; only the toggle button remains in the corner. */
					// Header — ESP Preview title, aligned like the settings
				// child headers so it feels part of the same design language.
				bypass::overlay::text_ex(
					L"",
					{ side_x + 20.0f, side_y + 22.0f, 20.0f, 20.0f },
					bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b,
						static_cast< std::uint8_t >( col_accent.a * content_alpha_f ) },
					14.0f, bypass::overlay::text_weight_t::regular,
					L"Font Awesome", bypass::overlay::text_align_t::left, false );
				bypass::overlay::text_ex(
					L"ESP Preview",
					{ side_x + 46.0f, side_y + 20.0f, side_w - 80.0f, 22.0f },
					bypass::overlay::color_t { 236, 232, 243,
						static_cast< std::uint8_t >( 235.0f * content_alpha_f ) }, 15.0f,
					bypass::overlay::text_weight_t::bold,
					L"Geist-Bold", bypass::overlay::text_align_t::left, false );
				// Accent-tinted faded divider — 64-segment gradient with
				// smoothstep alpha rolloff at the outer edges (same
				// pattern the profile / configuration cards use), but
				// coloured with the menu accent so it reads as part of
				// the theme rather than a neutral hairline. Alpha also
				// multiplied by content fade so it dissolves during
				// the minimise animation.
				{
					constexpr int   segments  = 64;
					constexpr float edge_fade = 0.38f;
					constexpr float max_alpha = 140.0f;
					const float div_x = side_x + 16.0f;
					const float div_w = side_w - 32.0f;
					const float div_y = side_y + 54.0f;
					if ( div_w > 8.0f ) {
						for ( int i = 0; i < segments; ++i ) {
							const float x0 = div_x + div_w * ( static_cast< float >( i ) / segments );
							const float x1 = div_x + div_w * ( static_cast< float >( i + 1 ) / segments );
							const float pos = ( static_cast< float >( i ) + 0.5f ) / segments;
							float fade = ( std::min )( pos, 1.0f - pos ) / edge_fade;
							fade = ( std::clamp )( fade, 0.0f, 1.0f );
							fade = fade * fade * ( 3.0f - 2.0f * fade );
							const auto a = static_cast< std::uint8_t >( max_alpha * fade * content_alpha_f );
							if ( a > 0 )
								bypass::overlay::filled_rectangle(
									{ x0, div_y, x1 - x0, 1.0f },
									bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, a } );
						}
					}
				}

#endif  /* end of #if 0 header block */

				// Child-style header (2026-08-26 refresh) — same layout
				// as the draw_child lambda around line 1321: accent
				// eye icon on the left, Geist-Bold title next to it,
				// faded 64-segment divider below. Right-side ellipsis
				// is replaced by the existing toggle button, which is
				// vertically centred in the same header band.
				//
				// Header band shrunk to 38 px (from 48) — user asked
				// for the divider higher — with the icon + toggle
				// re-centred inside the tighter band. Divider is now
				// tinted with the menu accent colour and given a
				// max_alpha bump so the fade in/out at the sides is
				// clearly visible against the panel background. Icon
				// is the Font Awesome eye (), the same glyph the
				// Boxes child card in the visuals tab uses.
				{
					constexpr float cp_hdr_h_esp    = 38.0f;
					constexpr float cp_icon_box_esp = 26.0f;
					constexpr float cp_pad_esp      = 12.0f;

					const float hdr_icon_x = side_x + 11.0f;
					const float hdr_icon_y = side_y + ( cp_hdr_h_esp - cp_icon_box_esp ) * 0.5f;
					// Explicit \xF06E so the glyph survives file
					// round-trips — see the same escape at line 80
					// for the Boxes card.
					// Explicit \xF06E escape so the glyph survives file
					// round-trips — same escape the Boxes card uses at
					// line 80 for its icon.
					static const wchar_t s_hdr_eye_icon[ 2 ] = { L'\xF06E', L'\0' };
					bypass::overlay::text_ex(
						s_hdr_eye_icon,   // Font Awesome eye — matches Boxes card icon (Font Awesome eye )
						{ hdr_icon_x, hdr_icon_y, cp_icon_box_esp, cp_icon_box_esp },
						bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b,
							static_cast< std::uint8_t >( col_accent.a * content_alpha_f ) },
						15.0f, bypass::overlay::text_weight_t::regular,
						L"Font Awesome", bypass::overlay::text_align_t::center, false );

					const float hdr_text_x    = hdr_icon_x + cp_icon_box_esp + 4.0f;
					const float hdr_text_maxw = side_w - ( hdr_text_x - side_x ) - cp_pad_esp - 28.0f;
					bypass::overlay::text_ex(
						L"ESP Preview",
						{ hdr_text_x, side_y, hdr_text_maxw, cp_hdr_h_esp },
						bypass::overlay::color_t { 245, 243, 249,
							static_cast< std::uint8_t >( 235.0f * content_alpha_f ) },
						14.0f, bypass::overlay::text_weight_t::black,
						L"Geist-Bold", bypass::overlay::text_align_t::left, false );

					// Faded divider — matched to the logo divider around
					// menu.cxx:7061 for the "prominent accent hairline"
					// look: 2 px thick, max_alpha 90, edge_fade 0.38.
					// Same 64-segment smoothstep so the fade in / fade
					// out on the sides matches the header under the
					// crisis logo, colour tied to col_accent.
					constexpr int   divider_segments  = 64;
					constexpr float divider_edge_fade = 0.38f;
					constexpr float divider_max_alpha = 90.0f;
					constexpr float divider_thickness = 2.0f;
					const float divider_x = side_x + cp_pad_esp;
					const float divider_w = side_w - cp_pad_esp * 2.0f;
					const float divider_y = side_y + cp_hdr_h_esp;
					if ( divider_w > 1.0f ) {
						for ( int i = 0; i < divider_segments; ++i ) {
							const float x0 = divider_x + divider_w * ( static_cast< float >( i ) / divider_segments );
							const float x1 = divider_x + divider_w * ( static_cast< float >( i + 1 ) / divider_segments );
							const float segment_w = x1 - x0;
							if ( segment_w <= 0.0f )
								continue;
							const float t    = ( static_cast< float >( i ) + 0.5f ) / static_cast< float >( divider_segments );
							const float side = ( std::min )( t, 1.0f - t );
							float fade       = side / divider_edge_fade;
							fade             = ( std::clamp )( fade, 0.0f, 1.0f );
							fade             = fade * fade * ( 3.0f - 2.0f * fade );
							const auto alpha = static_cast< std::uint8_t >( divider_max_alpha * fade * content_alpha_f );
							if ( alpha == 0 )
								continue;
							bypass::overlay::filled_rectangle(
								{ x0, divider_y, segment_w, divider_thickness },
								bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, alpha } );
						}
					}
				}

				// Toggle button — fixed 6 px inset from the card's right
				// edge, so it slides in with the shrinking width and
				// ends up centred inside the minimised strip. Icon
				// indicates the ACTION (not the current state):
				//   expanded  →  chevron-left  (click to collapse)
				//   minimised →  chevron-right (click to open)
				constexpr float btn_size  = 24.0f;   // match child header ellipsis rect
				constexpr float btn_inset = 6.0f;
				constexpr float btn_gap   = 2.0f;    // gap between the two right-side buttons
				const float btn_x = side_x + side_w - btn_size - btn_inset;
				// Vertical centre of the button lerps between the
				// 38 px header band (expanded) and the shrunken 34 px
				// card (minimised) so the button stays visually
				// centred throughout the height animation.
				const float btn_y_expanded  = side_y + ( 38.0f - btn_size ) * 0.5f;
				const float btn_y_minimised = side_y + ( min_side_h - btn_size ) * 0.5f;
				const float btn_y = btn_y_expanded + ( btn_y_minimised - btn_y_expanded ) * t_eased;
				// Per-button hover + click resolved inside the
				// draw_icon_button lambda below (draws two side-by-
				// side icon buttons); the shared btn_hovered flag is
				// gone.

				// (background rounded-rect + border removed — the
				// child header's ellipsis button uses no background,
				// just the raw glyph. The two icon buttons below
				// follow that pattern.)
#if 0
				// Font Awesome chevron-left / chevron-right for minimise / expand.
				// (Overwriting the icon assignment below so the empty
				// literals get filled with proper glyphs via \u escapes.)
				const wchar_t* toggle_icon_glyph_minimize = L"";  // chevron-left glyph L"" declared just above
				const wchar_t* toggle_icon_glyph_expand   = L"";  // chevron-right
				// (nuked broken line via inserted comment marker) //--broken-line-follows--// const wchar_t* toggle_icon = s_esp_expanded ? L"" L"" /*chevron-left*/ + 0 /* : L"" /*chevron-right*/;  // remainder of line commented — was leftover from Unicode-match iteration // L"" : L""; (void) L" : L"";
#endif
				// Toggle icon glyphs built via char-code constants
				// (\xF053 = chevron-left, \xF054 = chevron-right) so
				// the source doesn't rely on string-literal Unicode
				// bytes that repeatedly got lost through iteration.
				static const wchar_t s_chev_left [ 2 ] = { L'\xF053', L'\0' };
				static const wchar_t s_chev_right[ 2 ] = { L'\xF054', L'\0' };
				// \xF06E = eye, \xF070 = eye-slash. Same convention
				// as the minimise chevrons: the glyph describes the
				// ACTION, not the current state, so it changes when
				// clicked.
				static const wchar_t s_eye       [ 2 ] = { L'\xF06E', L'\0' };
				static const wchar_t s_eye_slash [ 2 ] = { L'\xF070', L'\0' };

				// Shared draw+click helper. `state` is toggled on
				// click. Idle / hover colours match the child header
				// ellipsis: {225,221,234,105} at rest, pure white on
				// hover for clear feedback. No background, no border.
				auto draw_icon_button = [ & ] ( float bx, const wchar_t* icon, bool* state ) {
					const bool hovered = hit_test( mx, my, bx, btn_y, btn_size, btn_size );
					if ( clicked && hovered && state )
						*state = !*state;
					const auto colour = hovered
						? bypass::overlay::color_t { 255, 255, 255, 245 }
						: bypass::overlay::color_t { 225, 221, 234, 105 };
					bypass::overlay::text_ex(
						icon,
						{ bx, btn_y, btn_size, btn_size },
						colour,
						11.0f, bypass::overlay::text_weight_t::regular,
						L"Font Awesome", bypass::overlay::text_align_t::center, false );
				};

				// Right-most: minimise / expand chevron (always inside the card).
				const float btn_min_x = btn_x;
				const wchar_t* minimise_icon = s_esp_expanded ? s_chev_left : s_chev_right;
				draw_icon_button( btn_min_x, minimise_icon, &s_esp_expanded );

				// Eye toggle position (2026-08-26): lerps between
				// "inside the header, just left of the minimise" (fully
				// expanded) and "outside the card, floating to its
				// left" (fully minimised). At t_eased = 1 the eye is
				// clearly detached from the card, connected only by a
				// slim vertical accent divider drawn just below.
				constexpr float eye_outside_gap = 8.0f;   // eye ↔ card gap when minimised
				const float btn_eye_x_expanded  = btn_min_x - btn_size - btn_gap;
				const float btn_eye_x_minimised = side_x   - btn_size - eye_outside_gap;
				const float btn_eye_x = btn_eye_x_expanded + ( btn_eye_x_minimised - btn_eye_x_expanded ) * t_eased;
				const wchar_t* eye_icon = s_esp_preview_show_esp ? s_eye_slash : s_eye;
				draw_icon_button( btn_eye_x, eye_icon, &s_esp_preview_show_esp );

				// Vertical gray divider between the eye and the card
				// (only visible while minimised — alpha rides on
				// t_eased so it fades in as the card collapses). Slim
				// 1 px column with a top/bottom smoothstep edge fade
				// so it doesn't feel like a hard block. Sits halfway
				// between the eye button's right edge and the card's
				// left edge so it reads as the "separator" the user
				// asked for.
				if ( t_eased > 0.02f ) {
					const float eye_right = btn_eye_x + btn_size;
					const float div_x = ( eye_right + side_x ) * 0.5f - 0.5f;   // 1-px column centred in the gap
					const float div_top = btn_y - 2.0f;
					const float div_bot = btn_y + btn_size + 2.0f;
					const float div_h   = div_bot - div_top;
					constexpr int   div_segs      = 24;
					constexpr float div_edge_fade = 0.32f;
					constexpr float div_max_alpha = 150.0f;
					for ( int i = 0; i < div_segs; ++i ) {
						const float y0  = div_top + div_h * ( static_cast< float >( i ) / div_segs );
						const float y1  = div_top + div_h * ( static_cast< float >( i + 1 ) / div_segs );
						const float pos = ( static_cast< float >( i ) + 0.5f ) / div_segs;
						float fade      = ( std::min )( pos, 1.0f - pos ) / div_edge_fade;
						fade            = ( std::clamp )( fade, 0.0f, 1.0f );
						fade            = fade * fade * ( 3.0f - 2.0f * fade );
						const auto a    = static_cast< std::uint8_t >( div_max_alpha * fade * t_eased );
						if ( a > 0 )
							bypass::overlay::filled_rectangle(
								{ div_x, y0, 1.0f, y1 - y0 },
								bypass::overlay::color_t { 170, 170, 175, a } );
					}
				}

				// Model area — only draw while content is still visible
				// and the card is wide enough to hold a non-trivial
				// square. Opacity ties to the same content fade so the
				// 3D model dissolves in step with the header + divider.
				if ( content_alpha_f > 0.005f && side_w > btn_size + 60.0f ) {
					// Padding around the preview area. Top pad reserves
					// the 38 px child-style header band + 6 px gap so
					// the model preview begins cleanly beneath the
					// header divider. Header was 48 px; the divider
					// moved up so the top pad shrank in step.
					constexpr float preview_top_pad  = 44.0f;
					constexpr float preview_bot_pad  = 10.0f;
					constexpr float preview_side_pad = 6.0f;
					const float preview_w_avail = side_w - preview_side_pad * 2.0f;
					const float preview_h_avail = side_h - preview_top_pad - preview_bot_pad;

					// Preview rect is 1:2 aspect (twice as tall as wide) to
					// match scene3d's 1024×2048 render target. Fit the
					// largest such rect inside the available space, then
					// shrink 8 % — user asked for "tiny bit smaller" and
					// this leaves a small breathing gap around the
					// preview so the ESP overlay text doesn't hard-collide
					// with the card border.
					constexpr float kPreviewScale = 0.92f;
					const float w_from_h  = preview_h_avail * 0.5f;
					const float preview_w = ( std::min )( preview_w_avail, w_from_h ) * kPreviewScale;
					const float preview_h = preview_w * 2.0f;
					if ( preview_w > 8.0f ) {
						// Nudge the model down a couple of pixels off
						// the vertically-centred position so the head
						// sits a hair further below the header divider
						// and the ESP text stack up top gets a bit more
						// clearance from the divider.
						constexpr float kPreviewYNudge = 4.0f;
						const float preview_x = side_x + ( side_w - preview_w ) * 0.5f;
						const float preview_y = side_y + preview_top_pad
							+ ( preview_h_avail - preview_h ) * 0.5f + kPreviewYNudge;

						// Drag-to-rotate — Y-axis (yaw) only. Horizontal
						// mouse delta spins the model around world Y so
						// the user can see all sides. Vertical delta is
						// ignored. Yaw wraps freely (no clamp) — after a
						// full 360° sweep the model just keeps spinning
						// in the same direction, which is what feels
						// natural for character orbit. State persists
						// across frames; re-centre by minimising +
						// re-expanding the card.
						static float s_esp_yaw            = 0.0f;
						static bool  s_esp_dragging       = false;
						static float s_esp_drag_start_mx  = 0.0f;
						static float s_esp_drag_start_yaw = 0.0f;

						const bool over_preview = hit_test( mx, my,
							preview_x, preview_y, preview_w, preview_h );

						if ( !s_esp_dragging && clicked && over_preview ) {
							s_esp_dragging       = true;
							s_esp_drag_start_mx  = mx;
							s_esp_drag_start_yaw = s_esp_yaw;
						}
						if ( s_esp_dragging ) {
							if ( !lmb ) {
								s_esp_dragging = false;
							}
							else {
								// Sensitivity: ~180° over 262 px of drag,
								// so a full sweep across the preview does
								// roughly a half rotation.
								constexpr float sensitivity = 0.012f;
								s_esp_yaw = s_esp_drag_start_yaw + ( mx - s_esp_drag_start_mx ) * sensitivity;
							}
						}

						bypass::overlay::scene3d(
							{ preview_x, preview_y, preview_w, preview_h },
							col_accent,                              // accent / emissive tint
							0.0f, s_esp_yaw, 0.0f,                   // Y-only rotation (yaw); X (pitch) locked at 0
							content_alpha_f );                       // opacity fades with minimise

						// ── ESP OVERLAY (phases 1-3) ────────────────────
						// Every config::player::* toggle drives an
						// overlay element over the 3D preview.
						//
						// Phase 1: box / health bar / head circle drawn
						//   from STATIC character bounds computed off the
						//   known camera + Fortnite proportions.
						// Phase 2: skeleton bones from a hard-coded
						//   Fortnite-ish A-pose bone map, projected each
						//   frame through the SAME model rotation + camera
						//   that scene3d uses. No wire-protocol change
						//   needed — the maths mirror scene3d's transform
						//   locally in the menu. Rotates naturally with
						//   the yaw drag.
						// Phase 3: text stacks (name / weapon / distance
						//   etc.) go through esp::text_small so the
						//   preview + in-game ESP share one code path,
						//   and per-stack offsets in config are dragged
						//   live on the preview and applied everywhere.
						{
							constexpr float kCharTopRatio    = 0.033f;   // Y=+1 world → preview_h * this from top
							constexpr float kCharBottomRatio = 0.967f;
							constexpr float kCharWidthRatio  = 0.75f;    // Carbide X extent (~0.397 units) / visible frame half-width (0.535) → ~0.75 of preview_w

							// ── BONES first — box binds to bone extents ────────
								// Hand-tuned Fortnite-proportion bone set covering
								// the WIDEST joints (shoulders/elbows/hands/hips/
								// knees/feet). Projected each frame through the
								// same yaw + camera scene3d uses. Follow-up
								// (bottom of the response note): overlay-side wire
								// extension to push the .glb's ACTUAL joint
								// positions per frame so bones are perfectly
								// accurate to whichever character is loaded.
								struct bone_pt_t { float x, y, z; float sx, sy; bool valid; };
								bone_pt_t bones[] = {
									{  0.00f,  0.98f, 0.0f, 0,0,false },
									{  0.00f,  0.85f, 0.0f, 0,0,false },
									{  0.00f,  0.60f, 0.0f, 0,0,false },
									{  0.00f,  0.40f, 0.0f, 0,0,false },
									{  0.22f,  0.55f, 0.0f, 0,0,false },
									{ -0.22f,  0.55f, 0.0f, 0,0,false },
									{  0.32f,  0.25f, 0.0f, 0,0,false },
									{ -0.32f,  0.25f, 0.0f, 0,0,false },
									{  0.38f, -0.05f, 0.0f, 0,0,false },
									{ -0.38f, -0.05f, 0.0f, 0,0,false },
									{  0.00f, -0.15f, 0.0f, 0,0,false },
									{  0.12f, -0.15f, 0.0f, 0,0,false },
									{ -0.12f, -0.15f, 0.0f, 0,0,false },
									{  0.12f, -0.55f, 0.0f, 0,0,false },
									{ -0.12f, -0.55f, 0.0f, 0,0,false },
									{  0.14f, -0.98f, 0.0f, 0,0,false },
									{ -0.14f, -0.98f, 0.0f, 0,0,false },
								};
								{
									const float yaw_c = std::cos( s_esp_yaw );
									const float yaw_s = std::sin( s_esp_yaw );
									constexpr float kCamZ       = 4.15f;   // must match scene3d camera distance
									constexpr float kTanHalfFov = 0.30573f;
									constexpr float kAspect     = 0.5f;
									for ( auto& b : bones ) {
										const float rx =  b.x * yaw_c + b.z * yaw_s;
										const float ry =  b.y;
										const float rz = -b.x * yaw_s + b.z * yaw_c;
										const float vz = rz + kCamZ;
										if ( vz <= 0.1f ) { b.valid = false; continue; }
										const float ndc_x = rx / ( kAspect * kTanHalfFov * vz );
										const float ndc_y = ry / (           kTanHalfFov * vz );
										b.sx = preview_x + ( ndc_x * 0.5f + 0.5f ) * preview_w;
										b.sy = preview_y + ( 0.5f - ndc_y * 0.5f ) * preview_h;
										b.valid = true;
									}
								}

								// Box bounds derived from bone extents. Small
								// silhouette pad so the box hugs mesh thickness
								// past bones (arm width, hair). Mirrors
								// cache.hxx's skeleton-derived player-box.
								float _bmin_x = 1e30f, _bmax_x = -1e30f;
								float _bmin_y = 1e30f, _bmax_y = -1e30f;
								for ( const auto& b : bones ) {
									if ( !b.valid ) continue;
									if ( b.sx < _bmin_x ) _bmin_x = b.sx;
									if ( b.sx > _bmax_x ) _bmax_x = b.sx;
									if ( b.sy < _bmin_y ) _bmin_y = b.sy;
									if ( b.sy > _bmax_y ) _bmax_y = b.sy;
								}
								// Extra pad accommodates the breathing animation (chest bones
								// shift ~2% of body height at peak inhale) plus mesh
								// silhouette (arm thickness / hair extending past bones).
								constexpr float kSilhouettePad = 18.0f;
								const float char_top    = _bmin_y - kSilhouettePad;
							const float char_bottom = _bmax_y + kSilhouettePad;
							const float char_cx     = ( _bmin_x + _bmax_x ) * 0.5f;
							// TEMP: bone-based bounds override the ratio-based char rect below.
								// half_w unused now — bone bounds compute char_left/right directly.
									[[maybe_unused]] const float half_w = preview_w * kCharWidthRatio * 0.5f;
							const float char_left   = _bmin_x - kSilhouettePad;
							const float char_right  = _bmax_x + kSilhouettePad;
							const float box_w       = char_right - char_left;
							const float box_h       = char_bottom - char_top;

							const bypass::overlay::color_t esp_accent = col_accent;
							const bypass::overlay::color_t esp_outline{ 0, 0, 0, 180 };
							const bypass::overlay::color_t esp_white  { 245, 243, 249, 235 };

							// Multiply every colour's alpha by content_alpha_f
							// so the whole overlay fades with the card minimise.
							auto fade = [ & ]( bypass::overlay::color_t c ) -> bypass::overlay::color_t {
								c.a = static_cast< std::uint8_t >( c.a * content_alpha_f );
								return c;
							};

							// ESP visibility gate — the eye-slash button
							// in the header sets s_esp_preview_show_esp
							// false to hide box / bars / text / snapline
							// on the character. Only the raw 3D model +
							// drag-to-rotate stays live.
							if ( s_esp_preview_show_esp ) {

							// BOX ────────────────────────────────────────
							if ( config::player::box::filled ) {
								bypass::overlay::filled_rectangle(
									{ char_left, char_top, box_w, box_h },
									fade( bypass::overlay::color_t {
										esp_accent.r, esp_accent.g, esp_accent.b, 55 } ) );
							}
							if ( config::player::box::normal ) {
								if ( config::player::box::outline )
									bypass::overlay::rectangle(
										{ char_left - 1.0f, char_top - 1.0f, box_w + 2.0f, box_h + 2.0f },
										fade( esp_outline ), 1.0f );
								bypass::overlay::rectangle(
									{ char_left, char_top, box_w, box_h },
									fade( esp_accent ), 1.0f );
							}
							if ( config::player::box::corner ) {
								const float len = ( std::min )( box_w, box_h ) * 0.25f;
								auto seg = [ & ]( float x1, float y1, float x2, float y2 ) {
									if ( config::player::box::outline )
										bypass::overlay::line_async(
											{ x1, y1 }, { x2, y2 }, fade( esp_outline ), 2.5f );
									bypass::overlay::line_async(
										{ x1, y1 }, { x2, y2 }, fade( esp_accent ), 1.5f );
								};
								seg( char_left,  char_top,    char_left  + len, char_top    );
								seg( char_left,  char_top,    char_left,        char_top    + len );
								seg( char_right, char_top,    char_right - len, char_top    );
								seg( char_right, char_top,    char_right,       char_top    + len );
								seg( char_left,  char_bottom, char_left  + len, char_bottom );
								seg( char_left,  char_bottom, char_left,        char_bottom - len );
								seg( char_right, char_bottom, char_right - len, char_bottom );
								seg( char_right, char_bottom, char_right,       char_bottom - len );
							}

							// HEALTH + SHIELD BARS — position driven by
							// config::health_bar_position + shield_bar_position
							// (0=left, 1=right, 2=top, 3=bottom). Draggable
							// in the preview: LMB-down on a bar starts a
							// drag, release snaps to whichever side the
							// cursor ended up on.
							if ( config::player::stats::health_bar ) {
								static int s_bar_drag = -1;   // 0=health, 1=shield, -1=none
								constexpr float hb_thick_hp = 3.0f;
								constexpr float hb_thick_sh = 3.0f;
								constexpr float hb_gap      = 3.0f;
								constexpr float hp_frac     = 1.0f;
								constexpr float sh_frac     = 1.0f;

								// Compute bar rect for a given position.
								// `edge_gap` is the distance from the box
								// edge to the box-facing edge of the bar,
								// so a second bar on the same side can
								// stack outward by passing edge_gap =
								// hb_gap + first_bar_thickness + hb_gap.
								auto bar_rect = [ & ]( int pos, float thickness, float edge_gap ) -> bypass::overlay::rect_t {
									bypass::overlay::rect_t r{};
									if ( pos == 0 ) {
										r.x = char_left - edge_gap - thickness; r.y = char_top; r.width = thickness; r.height = box_h;
									} else if ( pos == 1 ) {
										r.x = char_right + edge_gap; r.y = char_top; r.width = thickness; r.height = box_h;
									} else if ( pos == 2 ) {
										r.x = char_left; r.y = char_top - edge_gap - thickness; r.width = box_w; r.height = thickness;
									} else {
										r.x = char_left; r.y = char_bottom + edge_gap; r.width = box_w; r.height = thickness;
									}
									return r;
								};

								auto draw_bar = [ & ]( int pos, float thickness, float edge_gap, float frac, bypass::overlay::color_t fill ) {
									const auto r = bar_rect( pos, thickness, edge_gap );
									// Outline pad
									bypass::overlay::filled_rectangle(
										{ r.x - 1.0f, r.y - 1.0f, r.width + 2.0f, r.height + 2.0f },
										fade( esp_outline ) );
									bypass::overlay::filled_rectangle( r,
										fade( bypass::overlay::color_t { 22, 19, 35, 230 } ) );
									// Fill portion — vertical: fill upward; horizontal: left→right.
									if ( pos <= 1 ) {
										const float fh = r.height * frac;
										bypass::overlay::filled_rectangle(
											{ r.x, r.y + r.height - fh, r.width, fh },
											fade( fill ) );
									} else {
										bypass::overlay::filled_rectangle(
											{ r.x, r.y, r.width * frac, r.height },
											fade( fill ) );
									}
								};

								int hp_pos = config::player::stats::health_bar_position;
								int sh_pos = config::player::stats::shield_bar_position;
								if ( hp_pos < 0 || hp_pos > 3 ) hp_pos = 0;
								if ( sh_pos < 0 || sh_pos > 3 ) sh_pos = 3;

								// Same-side stacking: health is always at
								// stack index 0 (closest to box). Shield
								// stacks OUTWARD by (health thickness +
								// gap) when it shares the side; otherwise
								// it sits at its own edge_gap == hb_gap.
								const float hp_edge = hb_gap;
								const float sh_edge = ( sh_pos == hp_pos )
									? hb_gap + hb_thick_hp + hb_gap
									: hb_gap;

								// Health first (green).
								draw_bar( hp_pos, hb_thick_hp, hp_edge, hp_frac,
									bypass::overlay::color_t { 100, 220, 100, 255 } );
								// Shield second (blue).
								draw_bar( sh_pos, hb_thick_sh, sh_edge, sh_frac,
									bypass::overlay::color_t { 96, 176, 232, 255 } );

								// Drag start on LMB-down over either bar.
								if ( s_bar_drag < 0 && clicked ) {
									const auto hp_r = bar_rect( hp_pos, hb_thick_hp, hp_edge );
									const auto sh_r = bar_rect( sh_pos, hb_thick_sh, sh_edge );
									if ( hit_test( mx, my, hp_r.x - 2.f, hp_r.y - 2.f, hp_r.width + 4.f, hp_r.height + 4.f ) )
										s_bar_drag = 0;
									else if ( hit_test( mx, my, sh_r.x - 2.f, sh_r.y - 2.f, sh_r.width + 4.f, sh_r.height + 4.f ) )
										s_bar_drag = 1;
									// Consume the click: same frame the yaw
									// drag may have started; cancel it and
									// restore the pre-click yaw so the model
									// doesn't spin while a bar is dragged.
									if ( s_bar_drag >= 0 ) {
										s_esp_dragging = false;
										s_esp_yaw      = s_esp_drag_start_yaw;
									}
								}
								// Drag ghost — draws a semi-transparent bar
								// at the side of the box the cursor is
								// currently over. If the dragged bar would
								// land on the SAME side as the other bar,
								// the ghost stacks outward from the box so
								// the user sees where it will actually sit
								// after release (not overlapping the other
								// bar's rect).
								if ( s_bar_drag >= 0 && lmb ) {
									int hover_side = -1;
									if      ( my < char_top )    hover_side = 2;
									else if ( my > char_bottom ) hover_side = 3;
									else if ( mx < char_left )   hover_side = 0;
									else if ( mx > char_right )  hover_side = 1;
									if ( hover_side >= 0 ) {
										const float thick = ( s_bar_drag == 0 ) ? hb_thick_hp : hb_thick_sh;
										// Would-land-same-side detection:
										// health is being dragged (s_bar_drag==0)
										// → compare against shield's fixed pos.
										// Shield is being dragged (s_bar_drag==1)
										// → compare against health's fixed pos.
										const int other_pos = ( s_bar_drag == 0 ) ? sh_pos : hp_pos;
										const float other_thick = ( s_bar_drag == 0 ) ? hb_thick_sh : hb_thick_hp;
										const float ghost_edge = ( hover_side == other_pos )
											? hb_gap + other_thick + hb_gap
											: hb_gap;
										const bypass::overlay::color_t col = ( s_bar_drag == 0 )
											? bypass::overlay::color_t { 100, 220, 100, 160 }
											: bypass::overlay::color_t { 96, 176, 232, 160 };
										const auto ghost = bar_rect( hover_side, thick, ghost_edge );
										bypass::overlay::filled_rectangle( ghost, fade( col ) );
									}
								}
								// Release → commit new position.
								if ( s_bar_drag >= 0 && !lmb ) {
									int new_pos = -1;
									if      ( my < char_top )    new_pos = 2;
									else if ( my > char_bottom ) new_pos = 3;
									else if ( mx < char_left )   new_pos = 0;
									else if ( mx > char_right )  new_pos = 1;
									if ( new_pos >= 0 ) {
										if ( s_bar_drag == 0 ) config::player::stats::health_bar_position = new_pos;
										else                   config::player::stats::shield_bar_position = new_pos;
									}
									s_bar_drag = -1;
								}
							}

							// HEAD CIRCLE block removed 2026-08-26 —
							// the config toggle it referenced no
							// longer exists. Preview no longer draws
							// any head-circle overlay.
							if ( false ) {
								const float head_x = char_cx;
								const float head_y = char_top + box_h * 0.055f;
								const float r      = 5.0f;   // stub — dead branch, keeps below draw calls compilable if ever re-enabled
								bypass::overlay::circle( { head_x, head_y }, r + 1.5f, fade( esp_outline ), 1.5f );
								bypass::overlay::circle( { head_x, head_y }, r,        fade( esp_accent  ), 1.5f );
							}

							// SKELETON (phase 2 — bone-attached lines) ──
							// Disabled on the preview 2026-08-26 per user
							// request: the 3D character model already
							// makes the skeleton implicit, drawing extra
							// bone lines on top of the mesh looked noisy.
							// The in-game player.cxx ESP skeleton is
							// unaffected — it still honours all
							// config::player::skeleton::* toggles.
							if ( false && config::player::skeleton::normal ) {
								struct bone_pt_t { float x, y, z; float sx, sy; bool valid; };
								bone_pt_t bones[] = {
									{  0.00f,  0.85f, 0.0f, 0,0,false }, //  0 head
									{  0.00f,  0.60f, 0.0f, 0,0,false }, //  1 neck
									{  0.00f,  0.40f, 0.0f, 0,0,false }, //  2 chest
									{  0.20f,  0.55f, 0.0f, 0,0,false }, //  3 l_shoulder (char-left = world +X)
									{ -0.20f,  0.55f, 0.0f, 0,0,false }, //  4 r_shoulder
									{  0.30f,  0.25f, 0.0f, 0,0,false }, //  5 l_elbow
									{ -0.30f,  0.25f, 0.0f, 0,0,false }, //  6 r_elbow
									{  0.35f, -0.05f, 0.0f, 0,0,false }, //  7 l_hand
									{ -0.35f, -0.05f, 0.0f, 0,0,false }, //  8 r_hand
									{  0.00f, -0.15f, 0.0f, 0,0,false }, //  9 hip
									{  0.10f, -0.15f, 0.0f, 0,0,false }, // 10 l_hip
									{ -0.10f, -0.15f, 0.0f, 0,0,false }, // 11 r_hip
									{  0.10f, -0.55f, 0.0f, 0,0,false }, // 12 l_knee
									{ -0.10f, -0.55f, 0.0f, 0,0,false }, // 13 r_knee
									{  0.10f, -0.98f, 0.0f, 0,0,false }, // 14 l_foot
									{ -0.10f, -0.98f, 0.0f, 0,0,false }, // 15 r_foot
								};

								const float yaw_c = std::cos( s_esp_yaw );
								const float yaw_s = std::sin( s_esp_yaw );
								constexpr float kCamZ         = 4.15f;
								constexpr float kTanHalfFov   = 0.30573f;  // tan(17°)
								constexpr float kAspect       = 0.5f;      // scene3d target 1024×2048

								for ( auto& b : bones ) {
									// Y-axis rotation matching scene3d's rot matrix.
									const float rx =  b.x * yaw_c + b.z * yaw_s;
									const float ry =  b.y;
									const float rz = -b.x * yaw_s + b.z * yaw_c;
									// Camera at (0,0,-3.5) looking down +Z, view-space Z = world_z + camZ.
									const float vz = rz + kCamZ;
									if ( vz <= 0.1f ) { b.valid = false; continue; }
									const float ndc_x = rx / ( kAspect * kTanHalfFov * vz );
									const float ndc_y = ry / (           kTanHalfFov * vz );
									b.sx = preview_x + ( ndc_x * 0.5f + 0.5f ) * preview_w;
									b.sy = preview_y + ( 0.5f - ndc_y * 0.5f ) * preview_h;
									b.valid = true;
								}

								struct edge_t { int a, b; };
								const edge_t edges[] = {
									{ 0, 1 },   // head -> neck
									{ 1, 2 },   // neck -> chest
									{ 1, 3 }, { 1, 4 },   // neck -> shoulders
									{ 3, 5 }, { 5, 7 },   // left arm chain
									{ 4, 6 }, { 6, 8 },   // right arm chain
									{ 2, 9 },   // chest -> hip
									{ 9, 10 }, { 9, 11 }, // hip pivot -> hips
									{ 10, 12 }, { 12, 14 },// left leg chain
									{ 11, 13 }, { 13, 15 },// right leg chain
								};
								for ( const auto& e : edges ) {
									if ( !bones[ e.a ].valid || !bones[ e.b ].valid ) continue;
									const bypass::overlay::point_t p0 { bones[ e.a ].sx, bones[ e.a ].sy };
									const bypass::overlay::point_t p1 { bones[ e.b ].sx, bones[ e.b ].sy };
									if ( config::player::skeleton::outline )
										bypass::overlay::line_async( p0, p1, fade( esp_outline ), 2.5f );
									bypass::overlay::line_async( p0, p1, fade( esp_accent ), 1.0f );
								}
							}

							// TEXT SLOTS — drawn via the slot-aware helper
							// so anchor + order come from config, and
							// disabled slots collapse gaps automatically.
							// Compute per-frame position map first.
							gui::esp::begin_frame( );
							gui::esp::layout_t esp_layout {};
							esp_layout.box = { char_left, char_top, box_w, box_h };

							// Slot → (label, colour) — same content the
							// in-game player.cxx would generate for a
							// representative player. If a config toggle
							// is off, esp::text_small_slot no-ops.
							struct preview_slot_t {
								int slot;
								const wchar_t* label;
								bypass::overlay::color_t color;
							};
							// Preview slots correspond 1:1 to text_small_slot /
							// text_medium_slot call sites in player.cxx.
							// winrate / streamsnipe are intentionally
							// omitted because in-game those are drawn as
							// non-text elements (an icon + brand badge)
							// and don't participate in the text-stack
							// ordering.
							// Labels + colours mirror what player.cxx produces
							// in-game — hit-marker rank on the KD (Pro / Ace /
							// Champ prefixes), platform-tinted platform, colour-
							// tinted rank etc. Preview uses accent for the name
							// and distance (the two "primary" texts driven by
							// col_primary in-game). The other texts use the
							// same tint constants player.cxx hardcodes so what
							// you see here is what you get in the game.
							// Weapon-name label is built dynamically so weapon_ammo,
							// if enabled, appends onto the same line (matches
							// the compact style the user asked for). State is
							// omitted entirely from the preview.
							const wchar_t* weapon_label =
								config::player::stats::weapon_ammo
								? L"Legendary Assault Rifle  30 / 90"
								: L"Legendary Assault Rifle";

							const preview_slot_t preview_slots[] = {
								{ config::player::stats::slot_name,          L"MysticSniper42", esp_accent },
								{ config::player::stats::slot_platform,      L"Playstation",    { 0, 144, 193, 255 } },
								{ config::player::stats::slot_rank,          L"Champion",       { 221, 86, 20, 255 } },
								{ config::player::stats::slot_kd,            L"Pro 4.2 K/D",    { 232, 78, 78, 255 } },
								{ config::player::stats::slot_battle_pass,   L"BP Lv 200",      { 220, 180, 60, 255 } },
								{ config::player::stats::slot_distance,      L"42 m",           esp_accent },
								{ config::player::stats::slot_weapon_name,   weapon_label,      { 190, 190, 190, 255 } },
								{ config::player::stats::slot_weapon_damage, L"48 dmg",         { 255, 70, 70, 255 } },
								// Player state — matches the Skydiving colour player.cxx returns.
								{ config::player::stats::slot_state,         L"Skydiving",      { 80, 200, 240, 255 } },
								// Reloading tag — accent-coloured, gated by weapon_reloading toggle.
								{ config::player::stats::slot_reloading,     L"Reloading",      esp_accent },
							};
							// Manual sort + draw at a SMALLER preview size (user
							// asked for tiny smaller). Same stacking / order
							// semantics as flush_texts, but bypasses it so we
							// can pick our own size and capture each text's
							// rect for the drag hit-test below.
							constexpr float kPreviewFontSize = 10.5f;   // ~2px smaller than default 13.5 / 15
							constexpr float kPreviewLineGap  = 1.5f;
							constexpr float kPreviewTopGap   = 4.5f;    // small breathing room above box before first text
							constexpr float kPreviewBotGap   = 4.5f;    // small breathing room below box before first text
							// If the shield bar is drawn under the box
							// (1.5 px shield gap + 3 px shield height =
							// 4.5 px), the bottom text has to sit BELOW
							// that stack — otherwise the first bottom
							// text overlaps the shield bar. Scaled down
							// again (user asked for less gap around the
							// shield): 4.5 px shield stack + 0.5 px
							// extra buffer.
							constexpr float kShieldStackH    = 4.5f + 0.5f;   // 4.5 px shield stack + 0.5 px gap after
							constexpr float kPreviewSideGap  = 3.0f;
							constexpr float kPreviewSideAreaW = 110.0f;
							constexpr float kPreviewAreaW    = 160.0f;

							// Per-anchor bar offsets so text stacks on the
							// SAME side as a bar clear the bar. When both
							// bars land on one side they stack outward,
							// so we add ~one bar's stride per additional
							// bar (~thickness 3 + gap 3 = 6 px).
							constexpr float kPreviewHbOffset = 8.0f;
							constexpr float kPreviewHbStride = 6.0f;
							auto side_offset = [ ]( int side ) -> float {
								if ( !config::player::stats::health_bar ) return 0.f;
								const int hp = config::player::stats::health_bar_position;
								const int sh = config::player::stats::shield_bar_position;
								int n = 0;
								if ( hp == side ) ++n;
								if ( sh == side ) ++n;
								return n > 0 ? kPreviewHbOffset + ( n - 1 ) * kPreviewHbStride : 0.f;
							};
							const float left_hb_offset  = side_offset( 0 );
							const float right_hb_offset = side_offset( 1 );
							const float top_hb_offset   = side_offset( 2 );
							const float bot_shield_offset = side_offset( 3 );

							constexpr int   kSlotCount       = config::player::stats::slot_count;
							bypass::overlay::rect_t s_rects[ kSlotCount ] {};

							// Per-slot preview text size — uses in-game
							// size_for_slot() shrunk 2 px so it fits in
							// the preview widget.
							auto preview_size_for_slot = []( int slot ) {
								return gui::esp::size_for_slot( slot ) - 2.f;
							};

							// ── REORDER via DRAG (2026-09-04 rewrite) ────
							// The dragged slot INSERTS itself into the
							// effective layout at whatever position the
							// cursor is pointing at. All other slots
							// reflow around the insertion in real time
							// (smoothly interpolated). On drop the
							// current effective layout becomes final —
							// no separate "compute target on release"
							// step, so what you see during the drag is
							// exactly what commits.
							//
							// Behaviour matrix for target selection:
							//   • Cursor over ANOTHER slot's rect →
							//     insert AT that slot's position (if the
							//     cursor is on the box-side half) or one
							//     past it (if on the far-from-box half).
							//     Either way the hovered slot is bumped
							//     one position away from the box to make
							//     room, matching the user's "replace"
							//     intent.
							//   • Cursor in the GAP between two slots →
							//     insert between them; the far-from-box
							//     slot slides further out, the closer
							//     slot holds its position.
							//   • Cursor in empty space near a box edge
							//     → append to that edge's list.
							static int s_drag_slot = -1;

							// Cached visual rects — the actual rects the
							// slots render at. Each frame we lerp them
							// toward the freshly-computed logical rects
							// so a reorder animates smoothly instead of
							// snapping. Cleared when a slot's anchor
							// changes so newly-appearing slots don't
							// slide in from wherever they last were.
							struct visual_state_t {
								bypass::overlay::rect_t rect { };
								int   anchor { -1 };
								bool  seeded { false };
							};
							static visual_state_t s_visual[ kSlotCount ] { };

							// Namespace-scoped dt for the animation
							// interpolator (independent of any other
							// per-block timer so the animation keeps
							// ticking even while other UI blocks skip).
							static auto s_reflow_last_t = std::chrono::steady_clock::now( );

							// Sort helper — insertion-sort by
							// (anchor, order), reads effective values so
							// the same helper serves the baseline pass
							// AND the with-dragged-inserted pass.
							auto sort_by_anchor_order = [ & ] (
								int* idx, int n,
								const int* eff_anchor, const int* eff_order ) {
									for ( int i = 1; i < n; ++i ) {
										int k = idx[ i ];
										int ka = eff_anchor[ preview_slots[ k ].slot ];
										int ko = eff_order [ preview_slots[ k ].slot ];
										int j = i - 1;
										while ( j >= 0 ) {
											int ja = eff_anchor[ preview_slots[ idx[ j ] ].slot ];
											int jo = eff_order [ preview_slots[ idx[ j ] ].slot ];
											if ( ja < ka || ( ja == ka && jo <= ko ) ) break;
											idx[ j + 1 ] = idx[ j ]; --j;
										}
										idx[ j + 1 ] = k;
									}
								};

							// 4-anchor rect-computer — populates rects[]
							// (indexed by config slot enum) for every
							// enabled slot in a given (anchor, order)
							// arrangement. Used for both the baseline
							// hit-test pass and the final draw pass.
							auto compute_rects = [ & ] (
								const int* eff_anchor, const int* eff_order,
								bypass::overlay::rect_t* rects_out,
								bypass::overlay::text_align_t* aligns_out ) {
									int idx[ 32 ]; int n = 0;
									for ( int i = 0; i < static_cast< int >( std::size( preview_slots ) ); ++i )
										idx[ n++ ] = i;
									sort_by_anchor_order( idx, n, eff_anchor, eff_order );

									float cursors[ 4 ] = {
										kPreviewTopGap + top_hb_offset,
										kPreviewBotGap + bot_shield_offset,
										kPreviewTopGap,
										kPreviewTopGap,
									};
									for ( int i = 0; i < n; ++i ) {
										const auto& s = preview_slots[ idx[ i ] ];
										if ( !gui::esp::slot_enabled( s.slot ) ) continue;
										int anchor = eff_anchor[ s.slot ];
										if ( anchor < 0 || anchor > 3 ) anchor = 0;

										const float sz     = preview_size_for_slot( s.slot );
										const float text_h = sz + 4.f;
										const float step   = sz + kPreviewLineGap;

										bypass::overlay::rect_t r { };
										bypass::overlay::text_align_t align =
											bypass::overlay::text_align_t::center;
										if ( anchor == 0 ) {
											const float y = char_top - cursors[ 0 ] - sz;
											cursors[ 0 ] += step;
											r = { char_cx - kPreviewAreaW * 0.5f, y, kPreviewAreaW, text_h };
										}
										else if ( anchor == 1 ) {
											const float y = char_bottom + cursors[ 1 ];
											cursors[ 1 ] += step;
											r = { char_cx - kPreviewAreaW * 0.5f, y, kPreviewAreaW, text_h };
										}
										else if ( anchor == 2 ) {
											const float y = char_top + cursors[ 2 ];
											cursors[ 2 ] += step;
											const float right_edge = char_left - kPreviewSideGap - left_hb_offset;
											r = { right_edge - kPreviewSideAreaW, y, kPreviewSideAreaW, text_h };
											align = bypass::overlay::text_align_t::right;
										}
										else {
											const float y = char_top + cursors[ 3 ];
											cursors[ 3 ] += step;
											const float left_edge = char_right + kPreviewSideGap + right_hb_offset;
											r = { left_edge, y, kPreviewSideAreaW, text_h };
											align = bypass::overlay::text_align_t::left;
										}
										rects_out [ s.slot ] = r;
										aligns_out[ s.slot ] = align;
									}
								};

							// ── Effective anchor/order arrays ────────────
							// Start with config values. If a drag is in
							// progress we override the dragged slot's
							// (anchor, order) with the current target
							// insertion and renumber the target anchor's
							// list so the layout below reflects the drop
							// preview.
							int eff_anchor[ kSlotCount ];
							int eff_order [ kSlotCount ];
							for ( int i = 0; i < kSlotCount; ++i ) {
								eff_anchor[ i ] = config::player::stats::text_anchors[ i ];
								eff_order [ i ] = config::player::stats::text_orders [ i ];
							}
							bypass::overlay::text_align_t s_aligns[ kSlotCount ] { };

							// ── Baseline pass (no dragged) ───────────────
							// When dragging, produce a rects table WITHOUT
							// the dragged slot so we can hit-test the
							// mouse against the CURRENT layout to decide
							// where to insert. If not dragging this pass
							// is skipped entirely and s_rects goes
							// straight to the final layout below.
							bypass::overlay::rect_t baseline_rects[ kSlotCount ] { };
							if ( s_drag_slot >= 0 ) {
								// Temporarily lift the dragged slot out
								// by giving it an unreachable anchor. The
								// rect-computer skips disabled slots but
								// still needs the slot present in
								// preview_slots — so instead of removing
								// it, we mark it as anchor -1 which the
								// computer treats as "keep out of every
								// list". Actually simpler: skip by
								// checking eff_anchor inside the walker.
								//
								// Cleanest: run compute_rects with an
								// eff_anchor table where dragged = 99
								// (out-of-range → falls into the "else"
								// branch which reads cursors[ 3 ] — still
								// produces a rect but nothing hit-tests
								// against it because we'll never look up
								// baseline_rects[ s_drag_slot ]). To keep
								// the layout of the OTHER slots correct,
								// bump the dragged slot's order past
								// every real order so it lands after the
								// stack and doesn't perturb spacing.
								int base_anchor[ kSlotCount ];
								int base_order [ kSlotCount ];
								for ( int i = 0; i < kSlotCount; ++i ) {
									base_anchor[ i ] = eff_anchor[ i ];
									base_order [ i ] = eff_order [ i ];
								}
								base_anchor[ s_drag_slot ] = 3;   // parked
								base_order [ s_drag_slot ] = 9999;
								bypass::overlay::text_align_t discard[ kSlotCount ] { };
								compute_rects( base_anchor, base_order,
									baseline_rects, discard );
								baseline_rects[ s_drag_slot ] = { };   // don't hit-test this
							}

							// ── Determine drag target ────────────────────
							// Two-stage: (1) pick the target anchor,
							// (2) pick the target index by SPATIAL
							// position within that anchor's stack. The
							// spatial pass counts how many of the
							// anchor's items the cursor is "past"
							// (further-from-box side of), which makes
							// every index reachable — including the
							// topmost slot when the cursor is beyond the
							// stack, and index 0 when the cursor is
							// right next to the box. Replaces the older
							// "rect-half hit test + append fallback"
							// which couldn't reach the last positions
							// when the cursor moved past the stack.
							int drag_target_anchor = -1;
							int drag_target_index  = -1;
							if ( s_drag_slot >= 0 ) {
								// (1) Target anchor. Cursor over a slot
								// wins (that slot's anchor); otherwise
								// use a spatial zone around the box.
								int hovered_anchor_hint = -1;
								for ( const auto& s : preview_slots ) {
									if ( s.slot == s_drag_slot ) continue;
									if ( !gui::esp::slot_enabled( s.slot ) ) continue;
									const auto& r = baseline_rects[ s.slot ];
									if ( r.width <= 0.f ) continue;
									if ( hit_test( mx, my, r.x, r.y, r.width, r.height ) ) {
										hovered_anchor_hint = eff_anchor[ s.slot ];
										break;
									}
								}
								if ( hovered_anchor_hint >= 0 ) {
									drag_target_anchor = hovered_anchor_hint;
								}
								else {
									if      ( my < char_top    ) drag_target_anchor = 0;
									else if ( my > char_bottom ) drag_target_anchor = 1;
									else if ( mx < char_left   ) drag_target_anchor = 2;
									else if ( mx > char_right  ) drag_target_anchor = 3;
									else {
										const float dt_top    = my - char_top;
										const float dt_bottom = char_bottom - my;
										const float dt_left   = mx - char_left;
										const float dt_right  = char_right - mx;
										float best = dt_top; drag_target_anchor = 0;
										if ( dt_bottom < best ) { best = dt_bottom; drag_target_anchor = 1; }
										if ( dt_left   < best ) { best = dt_left;   drag_target_anchor = 2; }
										if ( dt_right  < best ) {                   drag_target_anchor = 3; }
									}
								}

								// (2) Build the sorted list for the
								// target anchor (excluding dragged) and
								// count how many items the cursor sits
								// past. Iteration order is index 0
								// (closest to box) upward. "Past" means
								// the cursor is on the far-from-box
								// side of the item's midpoint. Top
								// anchor stacks upward (higher index =
								// lower y), all others stack away with
								// increasing y — so the sign of the
								// comparison flips for the top case.
								int list[ kSlotCount ]; int ln = 0;
								for ( int i = 0; i < kSlotCount; ++i ) {
									if ( i == s_drag_slot ) continue;
									if ( eff_anchor[ i ] != drag_target_anchor ) continue;
									if ( !gui::esp::slot_enabled( i ) ) continue;
									list[ ln++ ] = i;
								}
								for ( int i = 1; i < ln; ++i ) {
									int k = list[ i ]; int ko = eff_order[ k ]; int j = i - 1;
									while ( j >= 0 && eff_order[ list[ j ] ] > ko ) {
										list[ j + 1 ] = list[ j ]; --j;
									}
									list[ j + 1 ] = k;
								}

								int insert_at = 0;
								if ( drag_target_anchor == 0 ) {
									// Top: index 0 has largest y (just
									// above box), higher index has
									// smaller y (further up). Cursor is
									// "past" an item when it sits above
									// (smaller y than) the item's mid.
									for ( int i = 0; i < ln; ++i ) {
										const auto& r = baseline_rects[ list[ i ] ];
										if ( my < r.y + r.height * 0.5f )
											insert_at = i + 1;
										else
											break;
									}
								}
								else {
									// Bottom / left / right: index 0 has
									// smallest y, higher index has
									// larger y. Cursor is "past" an
									// item when it sits below (larger y
									// than) the item's mid.
									for ( int i = 0; i < ln; ++i ) {
										const auto& r = baseline_rects[ list[ i ] ];
										if ( my > r.y + r.height * 0.5f )
											insert_at = i + 1;
										else
											break;
									}
								}
								drag_target_index = insert_at;

								// Apply the target to the effective
								// tables: insert dragged at target
								// position and renumber orders in the
								// target anchor.
								if ( insert_at < 0 )       insert_at = 0;
								if ( insert_at > ln )      insert_at = ln;
								for ( int i = ln; i > insert_at; --i )
									list[ i ] = list[ i - 1 ];
								list[ insert_at ] = s_drag_slot;
								++ln;
								for ( int i = 0; i < ln; ++i )
									eff_order[ list[ i ] ] = i;
								eff_anchor[ s_drag_slot ] = drag_target_anchor;
							}

							// ── Final logical layout (with dragged) ──────
							bypass::overlay::rect_t logical_rects[ kSlotCount ] { };
							compute_rects( eff_anchor, eff_order, logical_rects, s_aligns );

							// ── Interpolate visual rects toward logical ─
							// Frame-rate-independent exponential lerp
							// with a ~55 ms time constant — quick enough
							// to feel snappy, slow enough that the
							// reflow is visibly animated. Anchor changes
							// reset the visual rect to the new logical
							// value so a slot doesn't slide diagonally
							// across the widget when it jumps sides.
							{
								const auto now = std::chrono::steady_clock::now( );
								float dt = std::chrono::duration< float >(
									now - s_reflow_last_t ).count( );
								s_reflow_last_t = now;
								if ( dt < 0.f )    dt = 0.f;
								if ( dt > 0.25f )  dt = 0.25f;
								const float tau = 0.055f;
								// While the menu is being dragged, snap
								// visual rects straight to their logical
								// targets (t = 1) instead of lerping.
								// Reason: the menu's own frame moves 1:1
								// with the cursor, which shifts every
								// slot's logical rect on the same frame;
								// with the usual ~55 ms lerp the slots
								// visibly trail behind the menu chrome
								// and look like they're sliding around
								// mid-drag. Snapping keeps the preview
								// glued to the menu at every mouse-move
								// step and re-enables the smooth reflow
								// as soon as the drag ends.
								const float t = s_dragging
									? 1.0f
									: ( 1.f - std::exp( -dt / tau ) );

								for ( int i = 0; i < kSlotCount; ++i ) {
									if ( !gui::esp::slot_enabled( i ) ) {
										s_visual[ i ].seeded = false;
										s_visual[ i ].anchor = -1;
										s_visual[ i ].rect   = { };
										continue;
									}
									const auto& lg = logical_rects[ i ];
									if ( lg.width <= 0.f ) continue;
									auto& v = s_visual[ i ];
									if ( !v.seeded || v.anchor != eff_anchor[ i ] ) {
										v.rect   = lg;
										v.anchor = eff_anchor[ i ];
										v.seeded = true;
									}
									else {
										v.rect.x     += ( lg.x     - v.rect.x     ) * t;
										v.rect.y     += ( lg.y     - v.rect.y     ) * t;
										v.rect.width  = lg.width;
										v.rect.height = lg.height;
									}
									s_rects[ i ] = v.rect;
								}
							}

							// ── DRAW ─────────────────────────────────────
							// Walks preview_slots in effective sort order
							// so z-order matches the visual stack (later-
							// drawn = on top, so the dragged slot ends
							// up on top of any overlapping visual during
							// mid-animation). Uses the interpolated
							// visual rects, so a fresh drag or drop
							// still shows in-flight motion.
							{
								int draw_idx[ 32 ]; int dn = 0;
								for ( int i = 0; i < static_cast< int >( std::size( preview_slots ) ); ++i )
									draw_idx[ dn++ ] = i;
								sort_by_anchor_order( draw_idx, dn, eff_anchor, eff_order );

								for ( int i = 0; i < dn; ++i ) {
									const auto& s = preview_slots[ draw_idx[ i ] ];
									if ( !gui::esp::slot_enabled( s.slot ) ) continue;
									const auto& r = s_rects[ s.slot ];
									if ( r.width <= 0.f ) continue;
									const float sz = preview_size_for_slot( s.slot );
									int anchor = eff_anchor[ s.slot ];
									if ( anchor < 0 || anchor > 3 ) anchor = 0;
									const bypass::overlay::text_align_t align = s_aligns[ s.slot ];

									// Text-background frosted plate
									// (matches text.hxx draw_text_
									// background). Sized to the actual
									// glyph width, not the layout rect.
									// Suppressed for the dragged slot so
									// the floating item reads as a raw
									// glyph run — matches the "picked
									// up out of the stack" feel.
									if ( s.slot != s_drag_slot &&
										 config::player::stats::text_background_enabled[ s.slot ] ) {
										std::size_t n = 0;
										while ( s.label && s.label[ n ] ) ++n;
										const float text_w = static_cast< float >( n ) * sz * 0.60f;
										if ( text_w > 0.f ) {
											float text_x = r.x;
											if ( anchor == 0 || anchor == 1 )
												text_x = r.x + ( r.width - text_w ) * 0.5f;
											else if ( anchor == 2 )
												text_x = r.x + r.width - text_w;
											const bypass::overlay::rect_t plate {
												text_x - 3.f, r.y + 1.f, text_w + 6.f, r.height - 2.f };
											bypass::overlay::frosted_panel( plate, 4.f, 6.0f,
												fade( bypass::overlay::color_t {
													s.color.r, s.color.g, s.color.b, 140u } ) );
											bypass::overlay::rounded_rectangle( plate, 4.f,
												fade( bypass::overlay::color_t {
													s.color.r, s.color.g, s.color.b, 200u } ),
												1.0f );
										}
									}

									// Per-slot weight: state slot ("Skydiving")
									// renders at REGULAR weight so it reads
									// as ambient status rather than a primary
									// stat. Every other slot stays semi_bold
									// to match the in-game hierarchy.
									const auto weight = ( s.slot == config::player::stats::slot_state )
										? bypass::overlay::text_weight_t::regular
										: bypass::overlay::text_weight_t::semi_bold;
									const wchar_t* font = ( gui::esp::size_for_slot( s.slot ) < gui::esp::text_size_regular )
										? L"FiraCode-Light" : L"FiraCode-Medium";
									bypass::overlay::text_ex(
										s.label, r, fade( s.color ), sz,
										weight,
										font, align, false );
								}
							}

							// ── GEAR BUTTON (session-work reconstruction) ─
							// Inline gear icon next to the selected/hovered
							// text. Two-click flow: click text or ghost
							// gear → mark slot active; click the bright
							// gear → toggle Font Customization child.
							// State vars mirror the FONT CUSTOMIZATION
							// block below (hoisted here so gear can
							// mutate them). Also serves as the anchor
							// for the child on first open.
							static int   s_sel_slot         = -1;
							static bool  s_settings_open    = false;
							static float s_settings_x       = 0.f;
							static float s_settings_y       = 0.f;
							static bool  s_settings_placed  = false;
							static bool  s_settings_drag    = false;
							static float s_settings_drag_dx = 0.f;
							static float s_settings_drag_dy = 0.f;
							// Ease-in/out for the child panel — bumps to
							// 1 while sep_want_open, drains to 0 on close;
							// smoothstep gives the S-curve.
							static float s_child_ease_t    = 0.f;
							static int   s_child_slot      = -1;   // snapshot of s_sel_slot for fade-out
							// Gear click-flare — kicked to 1 on Background
							// toggle, drains as an extra scale spike on
							// the selected gear (stand-in for the rotation
							// cue since text_ex has no rotation path).
							static float s_gear_flare_t    = 0.f;
							// Namespace-level dt for the child animations
							// (independent of the gear-render block so it
							// keeps ticking during fade-out).
							static auto  s_child_last_t    = std::chrono::steady_clock::now( );

							// Which slot is under the cursor (if any).
							int hover_slot = -1;
							if ( s_drag_slot < 0 ) {
								for ( const auto& s : preview_slots ) {
									const auto r = s_rects[ s.slot ];
									if ( r.width <= 0.f ) continue;
									if ( hit_test( mx, my, r.x, r.y, r.width, r.height ) ) {
										hover_slot = s.slot;
										break;
									}
								}
							}

							// Gear rect: sits INLINE at end of glyph run
							// (or before the first glyph for anchor 2 so
							// it stays on the outer edge).
							auto compute_gear_rect = [ & ]( int slot, float gear_size ) -> bypass::overlay::rect_t {
								if ( slot < 0 ) return {};
								const auto tr = s_rects[ slot ];
								if ( tr.width <= 0.f ) return {};
								const wchar_t* label = nullptr;
								for ( const auto& s : preview_slots )
									if ( s.slot == slot ) { label = s.label; break; }
								if ( !label ) return {};
								std::size_t n = 0; while ( label[ n ] ) ++n;
								const float sz = preview_size_for_slot( slot );
								const float text_w = static_cast< float >( n ) * sz * 0.60f;
								const int anchor = config::player::stats::text_anchors[ slot ];
								float text_start_x = tr.x;
								if ( anchor == 0 || anchor == 1 )
									text_start_x = tr.x + ( tr.width - text_w ) * 0.5f;
								else if ( anchor == 2 )
									text_start_x = tr.x + tr.width - text_w;
								const float y = tr.y + ( tr.height - gear_size ) * 0.5f;
								constexpr float kSelInlineGap = 6.f;
								if ( anchor == 2 )
									return { text_start_x - kSelInlineGap - gear_size, y, gear_size, gear_size };
								return { text_start_x + text_w + kSelInlineGap, y, gear_size, gear_size };
							};

							// Hover-preview ghost gear (dim) for whichever
							// slot the cursor is over, if it's not the
							// currently-selected one. Tinted to the
							// slot's own text colour so the gear reads
							// as "belonging to" that text rather than a
							// disembodied grey glyph.
							if ( hover_slot >= 0 && hover_slot != s_sel_slot ) {
								const float ghost_base = preview_size_for_slot( hover_slot );
								const float ghost_size = ghost_base * 1.05f;
								const auto  ghost_rect = compute_gear_rect( hover_slot, ghost_size );
								if ( ghost_rect.width > 0.f ) {
									// Look up the text colour for the
									// hovered slot; fall back to the
									// prior neutral grey if the lookup
									// fails (out-of-array slot id).
									bypass::overlay::color_t ghost_col { 205, 205, 215, 120u };
									for ( const auto& s : preview_slots ) {
										if ( s.slot == hover_slot ) {
											ghost_col = { s.color.r, s.color.g, s.color.b, 120u };
											break;
										}
									}
									static const wchar_t s_ghost_icon[ 2 ] = { L'\xF013', L'\0' };
									bypass::overlay::text_ex( s_ghost_icon, ghost_rect,
										fade( ghost_col ),
										ghost_size,
										bypass::overlay::text_weight_t::regular,
										L"Font Awesome",
										bypass::overlay::text_align_t::center, false );
								}
							}

							// Selected-slot bright gear with spring pop.
							bypass::overlay::rect_t sel_gear {};
							bool sel_gear_valid = false;
							if ( s_sel_slot >= 0 && s_drag_slot < 0 ) {
								const float base_size = preview_size_for_slot( s_sel_slot );
								const float idle_size = base_size * 1.05f;
								sel_gear = compute_gear_rect( s_sel_slot, idle_size );
								if ( sel_gear.width > 0.f ) {
									sel_gear_valid = true;
									const bool gear_hover = hit_test( mx, my,
										sel_gear.x, sel_gear.y, sel_gear.width, sel_gear.height );

									// Spring-eased pop-out. Stays lit when
									// settings child is open (visual anchor).
									static float s_gear_t = 0.f;
									static float s_gear_v = 0.f;
									static auto  s_gear_last_t = std::chrono::steady_clock::now( );
									const auto now_hp = std::chrono::steady_clock::now( );
									const float dt = ( std::min )(
										std::chrono::duration< float >( now_hp - s_gear_last_t ).count( ),
										0.05f );
									s_gear_last_t = now_hp;
									const bool  pop_on    = gear_hover || s_settings_open;
									const float target    = pop_on ? 1.f : 0.f;
									const float stiffness = pop_on ? 260.f : 150.f;
									const float damping   = pop_on ?  20.f :  22.f;
									const float ax = ( target - s_gear_t ) * stiffness - s_gear_v * damping;
									s_gear_v += ax * dt;
									s_gear_t += s_gear_v * dt;
									const float t_vis = ( std::clamp )( s_gear_t, -0.05f, 1.20f );
									const float t     = ( std::clamp )( s_gear_t,  0.f,   1.f   );
									// Drain click-flare (Background toggle
									// kick) and add as extra scale spike.
									s_gear_flare_t = ( std::max )( 0.f, s_gear_flare_t - dt * 4.5f );
									const float glyph_size = base_size * ( 1.05f + 0.50f * t_vis + 0.35f * s_gear_flare_t );

									// Idle + hover both derive from the
									// selected slot's own text colour so the
									// gear stays visually tied to the text it
									// controls. Idle is the text colour at
									// 220 alpha; hover pushes to full alpha
									// so it "brightens" on approach.
									bypass::overlay::color_t sel_text_col { 205, 205, 215, 255u };
									for ( const auto& s : preview_slots ) {
										if ( s.slot == s_sel_slot ) {
											sel_text_col = { s.color.r, s.color.g, s.color.b, 255u };
											break;
										}
									}
									const bypass::overlay::color_t idle_col  {
										sel_text_col.r, sel_text_col.g, sel_text_col.b, 220u };
									const bypass::overlay::color_t hover_col {
										sel_text_col.r, sel_text_col.g, sel_text_col.b, 255u };
									const auto lerp8 = [ ]( std::uint8_t a, std::uint8_t b, float u ) {
										return static_cast< std::uint8_t >( a + ( int( b ) - int( a ) ) * u );
									};
									const bypass::overlay::color_t glyph_col {
										lerp8( idle_col.r, hover_col.r, t ),
										lerp8( idle_col.g, hover_col.g, t ),
										lerp8( idle_col.b, hover_col.b, t ),
										lerp8( idle_col.a, hover_col.a, t ) };

									const bypass::overlay::rect_t draw_rect {
										( sel_gear.x + sel_gear.width  * 0.5f ) - glyph_size * 0.5f,
										( sel_gear.y + sel_gear.height * 0.5f ) - glyph_size * 0.5f,
										glyph_size, glyph_size };
									static const wchar_t s_gear_icon[ 2 ] = { L'\xF013', L'\0' };
									bypass::overlay::text_ex( s_gear_icon, draw_rect,
										fade( glyph_col ),
										glyph_size,
										bypass::overlay::text_weight_t::regular,
										L"Font Awesome",
										bypass::overlay::text_align_t::center, false );
								}
							}

							// Click handling — three-tier gate:
							//  (a) LMB on SELECTED gear → toggle settings.
							//  (b) LMB on HOVER (unselected) text or its
							//      ghost gear → mark slot active.
							//  (c) Otherwise fall through to drag start.
							bool consumed_click = false;
							if ( clicked && s_drag_slot < 0 ) {
								if ( sel_gear_valid && hit_test( mx, my,
									sel_gear.x, sel_gear.y, sel_gear.width, sel_gear.height ) ) {
									s_settings_open = !s_settings_open;
									if ( s_settings_open && !s_settings_placed ) {
										s_settings_x = sel_gear.x + sel_gear.width + 8.f;
										s_settings_y = sel_gear.y;
										s_settings_placed = true;
									}
									consumed_click = true;
								}
								else if ( hover_slot >= 0 && hover_slot != s_sel_slot ) {
									// Check hover gear hit first — else text.
									const float hs = preview_size_for_slot( hover_slot ) * 1.05f;
									const auto  hover_gr = compute_gear_rect( hover_slot, hs );
									if ( ( hover_gr.width > 0.f && hit_test( mx, my,
										hover_gr.x, hover_gr.y, hover_gr.width, hover_gr.height ) )
										|| hit_test( mx, my,
											s_rects[ hover_slot ].x, s_rects[ hover_slot ].y,
											s_rects[ hover_slot ].width, s_rects[ hover_slot ].height ) ) {
										s_sel_slot = hover_slot;
										consumed_click = true;
									}
								}
								// Click outside child + outside any preview
								// text → deselect + close.
								if ( !consumed_click && hover_slot < 0 ) {
									const bool over_child = s_settings_open && hit_test( mx, my,
										s_settings_x, s_settings_y, 232.f, 108.f );
									if ( !over_child ) {
										s_sel_slot      = -1;
										s_settings_open = false;
									}
								}
							}

							// Start a drag on LMB-down over any slot — but
							// only if no gear/text-select already consumed
							// the click above.
							if ( s_drag_slot < 0 && clicked && !consumed_click ) {
								for ( const auto& s : preview_slots ) {
									const auto r = s_rects[ s.slot ];
									if ( r.width <= 0.f ) continue;
									if ( hit_test( mx, my, r.x, r.y, r.width, r.height ) ) {
										s_drag_slot = s.slot;
										// Cancel any yaw drag started this same frame
										// so the model stops spinning during text drag.
										s_esp_dragging = false;
										s_esp_yaw      = s_esp_drag_start_yaw;
										break;
									}
								}
							}

							// On release, COMMIT whatever effective
							// arrangement we've been previewing. The
							// per-frame drag pass above already inserted
							// the dragged slot at drag_target_index in
							// drag_target_anchor and renumbered orders,
							// so all we do here is copy eff_anchor /
							// eff_order back into config. The visual
							// rects keep lerping toward the same targets
							// they were already heading for — the drop
							// is completely seamless, no re-snap.
							if ( s_drag_slot >= 0 && !lmb ) {
								for ( int i = 0; i < kSlotCount; ++i ) {
									config::player::stats::text_anchors[ i ] = eff_anchor[ i ];
									config::player::stats::text_orders [ i ] = eff_order [ i ];
								}
								s_drag_slot = -1;
							}

							// ── FONT CUSTOMIZATION CHILD ─────────────────
							// State vars declared in the gear-button block
							// above; opened by clicking the bright gear on
							// a selected slot, closed by clicking outside.
							// Right-click on any preview text is a
							// shortcut that both selects and opens.
							if ( right_clicked && s_drag_slot < 0 ) {
								for ( const auto& s : preview_slots ) {
									const auto r = s_rects[ s.slot ];
									if ( r.width <= 0.f ) continue;
									if ( hit_test( mx, my, r.x, r.y, r.width, r.height ) ) {
										s_sel_slot      = s.slot;
										s_settings_open = true;
										if ( !s_settings_placed ) {
											s_settings_x = r.x + r.width + 8.f;
											s_settings_y = r.y;
											s_settings_placed = true;
										}
										break;
									}
								}
							}

							// Advance child + separator + panel ease each
							// frame (independent of gear-render dt so it
							// keeps ticking during fade-out).
							{
								const auto now_hp = std::chrono::steady_clock::now( );
								const float dt = ( std::min )(
									std::chrono::duration< float >( now_hp - s_child_last_t ).count( ),
									0.05f );
								s_child_last_t = now_hp;
								const bool want_open = s_settings_open && s_sel_slot >= 0;
								if ( want_open ) s_child_slot = s_sel_slot;
								const float target = want_open ? 1.f : 0.f;
								s_child_ease_t += ( target - s_child_ease_t )
									* ( 1.f - std::exp( -dt * 12.f ) );
								if ( std::abs( target - s_child_ease_t ) < 0.002f ) s_child_ease_t = target;
							}

							// Render whenever ease > 0 (draws during
							// fade-out too). Interactions gated on the
							// REAL open state further down.
							const bool sep_want_open = ( s_settings_open && s_sel_slot >= 0 );
							if ( ( sep_want_open || s_child_ease_t > 0.001f ) && s_child_slot >= 0 ) {
								const int sel_slot_local = s_child_slot;
								constexpr float kCw = 232.f;
								constexpr float kCh = 108.f;

								// Smoothstep the child + separator ease for
								// the S-curve shape used by the main
								// menu's child panels.
								const float ease_p     = ( std::clamp )( s_child_ease_t, 0.f, 1.f );
								const float ease_eased = ease_p * ease_p * ( 3.f - 2.f * ease_p );
								const auto mul_a = [ ]( std::uint8_t a, float m ) {
									return static_cast< std::uint8_t >( a * ( std::clamp )( m, 0.f, 1.f ) );
								};
								// child_fade — wraps every colour so the
								// ENTIRE panel fades in/out together.
								const auto child_fade = [ & ]( bypass::overlay::color_t c ) {
									c.a = mul_a( c.a, ease_eased );
									return fade( c );
								};

								const bypass::overlay::rect_t child_rect  { s_settings_x, s_settings_y, kCw, kCh };
								const bypass::overlay::rect_t header_rect { s_settings_x, s_settings_y + 4.f, kCw, 22.f };

								bypass::overlay::filled_rounded_rectangle( child_rect, 10.f,
									child_fade( bypass::overlay::color_t { 22, 21, 40, 255u } ) );
								bypass::overlay::rounded_rectangle    ( child_rect, 10.f,
									child_fade( bypass::overlay::color_t { 62, 66, 84, 200u } ), 1.f );

								// Title — "Font Customization" in accent.
								bypass::overlay::text_ex( L"Font Customization", header_rect,
									child_fade( bypass::overlay::color_t {
										esp_accent.r, esp_accent.g, esp_accent.b, 245u } ),
									13.f, bypass::overlay::text_weight_t::black,
									L"Geist-Bold",
									bypass::overlay::text_align_t::center, false );

								// Accent separator — width tweens from 0 → full
								// on open, drains back on close (uses the
								// same smoothstep as the panel).
								const float sep_full  = kCw - 24.f;
								const float sep_width = sep_full * ease_eased;
								const float sep_x     = s_settings_x + ( kCw - sep_width ) * 0.5f;
								const bypass::overlay::rect_t sep_rect {
									sep_x, s_settings_y + 29.f,
									sep_width, 1.5f };
								bypass::overlay::filled_rounded_rectangle( sep_rect, 0.75f,
									child_fade( bypass::overlay::color_t {
										esp_accent.r, esp_accent.g, esp_accent.b, 200u } ) );

								// Row 1 — [-] Small/Medium/Regular [+].
								const bypass::overlay::rect_t minus_r { s_settings_x + 10.f, s_settings_y + 40.f, 20.f, 16.f };
								const bypass::overlay::rect_t plus_r  { s_settings_x + kCw - 30.f, s_settings_y + 40.f, 20.f, 16.f };
								const bypass::overlay::rect_t sz_lbl_r{ s_settings_x + 34.f, s_settings_y + 40.f, kCw - 68.f, 16.f };

								auto btn = [ & ]( const bypass::overlay::rect_t& r, const wchar_t* lbl ) {
									const bool hover = hit_test( mx, my, r.x, r.y, r.width, r.height );
									bypass::overlay::filled_rounded_rectangle( r, 3.f,
										child_fade( bypass::overlay::color_t { 22, 21, 40, static_cast< std::uint8_t >( hover ? 235u : 200u ) } ) );
									bypass::overlay::rounded_rectangle    ( r, 3.f,
										child_fade( col_border ), 1.f );
									bypass::overlay::text_ex( lbl, r,
										child_fade( bypass::overlay::color_t { 236, 232, 243, 235 } ),
										11.f, bypass::overlay::text_weight_t::regular,
										L"Geist-Bold",
										bypass::overlay::text_align_t::center, false );
									return hover;
								};

								const bool minus_hover = btn( minus_r, L"-" );
								const bool plus_hover  = btn( plus_r,  L"+" );

								static const wchar_t* kSizeNames[ 3 ] = { L"Small", L"Medium", L"Regular" };
								int& sz_ref = config::player::stats::text_sizes[ sel_slot_local ];
								if ( sz_ref < 0 ) sz_ref = 0;
								if ( sz_ref > 2 ) sz_ref = 2;
								bypass::overlay::text_ex( kSizeNames[ sz_ref ], sz_lbl_r,
									child_fade( bypass::overlay::color_t { 236, 232, 243, 235 } ),
									11.f, bypass::overlay::text_weight_t::semi_bold,
									L"Geist-Bold",
									bypass::overlay::text_align_t::center, false );

								// Row 2 — Background checkbox + label.
								const bypass::overlay::rect_t bg_r     { s_settings_x + 10.f, s_settings_y + 64.f, 16.f, 16.f };
								const bypass::overlay::rect_t bg_lbl_r { s_settings_x + 32.f, s_settings_y + 64.f, kCw - 42.f, 16.f };
								{
									bool& bg_on = config::player::stats::text_background_enabled[ sel_slot_local ];
									bypass::overlay::filled_rounded_rectangle( bg_r, 4.f,
										child_fade( bypass::overlay::color_t { 22, 21, 40, 255u } ) );
									bypass::overlay::rounded_rectangle    ( bg_r, 4.f,
										child_fade( col_border ), 1.f );
									if ( bg_on ) {
										constexpr float inner_scale = 0.72f;
										const float sz  = bg_r.width * inner_scale;
										const float off = ( bg_r.width - sz ) * 0.5f;
										bypass::overlay::filled_rounded_rectangle(
											{ bg_r.x + off, bg_r.y + off, sz, sz },
											4.f * inner_scale,
											child_fade( bypass::overlay::color_t {
												esp_accent.r, esp_accent.g, esp_accent.b, 230u } ) );
									}
									bypass::overlay::text_ex( L"Background", bg_lbl_r,
										child_fade( bypass::overlay::color_t { 236, 232, 243, 235 } ),
										11.f, bypass::overlay::text_weight_t::regular,
										L"Geist-Bold",
										bypass::overlay::text_align_t::right, false );
								}

								// Row 3: drag/close hint.
								bypass::overlay::text_ex( L"drag the header to move; click outside to close",
									{ s_settings_x + 8.f, s_settings_y + kCh - 20.f, kCw - 16.f, 14.f },
									child_fade( bypass::overlay::color_t { 150, 145, 165, 200 } ),
									9.5f, bypass::overlay::text_weight_t::bold,
									L"Geist-Bold",
									bypass::overlay::text_align_t::center, false );

								// ── Interaction ──
								// Gated on sep_want_open (real open state)
								// so clicks during the fade-out don't
								// re-toggle the Background checkbox or
								// start a header drag.
								if ( sep_want_open ) {
									if ( clicked && hit_test( mx, my, header_rect.x, header_rect.y, header_rect.width, header_rect.height ) ) {
										s_settings_drag = true;
										s_settings_drag_dx = mx - s_settings_x;
										s_settings_drag_dy = my - s_settings_y;
									}
									if ( s_settings_drag ) {
										if ( !lmb ) s_settings_drag = false;
										else {
											s_settings_x = mx - s_settings_drag_dx;
											s_settings_y = my - s_settings_drag_dy;
										}
									}
									if ( clicked && minus_hover ) sz_ref = ( sz_ref > 0 ) ? sz_ref - 1 : sz_ref;
									if ( clicked && plus_hover  ) sz_ref = ( sz_ref < 2 ) ? sz_ref + 1 : sz_ref;
									if ( clicked && ( hit_test( mx, my, bg_r.x, bg_r.y, bg_r.width, bg_r.height )
										|| hit_test( mx, my, bg_lbl_r.x, bg_lbl_r.y, bg_lbl_r.width, bg_lbl_r.height ) ) ) {
										config::player::stats::text_background_enabled[ sel_slot_local ] =
											!config::player::stats::text_background_enabled[ sel_slot_local ];
										// Kick the gear click-flare so the
										// Background toggle click reads as
										// confirmed (stand-in for the
										// rotation cue).
										s_gear_flare_t = 1.f;
									}
									// Click-outside close is handled by
									// the gear block above (which knows
									// about the gear rect too and won't
									// fire on gear clicks). Duplicating it
									// here would close the child on the
									// SAME frame the gear opens it because
									// the gear click position is outside
									// the child rect by geometry.
								}
							}

							// SNAPLINE (from configurable box anchor down) ─
							if ( config::player::snaplines::enabled ) {
								const float snap_from_x = char_cx;
								const float snap_from_y =
									config::player::snaplines::position == 0 ? char_top :
									config::player::snaplines::position == 1 ? ( char_top + char_bottom ) * 0.5f :
									                                            char_bottom;
								const float snap_to_x = preview_x + preview_w * 0.5f;
								const float snap_to_y = preview_y + preview_h;
								if ( config::player::snaplines::outline )
									bypass::overlay::line_async(
										{ snap_from_x, snap_from_y }, { snap_to_x, snap_to_y },
										fade( esp_outline ), 2.5f );
								bypass::overlay::line_async(
									{ snap_from_x, snap_from_y }, { snap_to_x, snap_to_y },
									fade( esp_accent ), 1.0f );
							}

							}  // end of `if ( s_esp_preview_show_esp )` gate
						}
					}
				}
			}

			for ( const auto& draw_dropdown : s_dropdown_draw_queue )
				draw_dropdown( );
			s_dropdown_draw_queue.clear( );

			// Tooltips (only when panel is visible)
			if ( hovered_main_tab >= 0 ) {
				const main_tab_t& tab = main_tabs[ hovered_main_tab ];
				// Tooltip anchor uses the same centred group start_y +
				// spacing constants the button loop above uses, so
				// hovering any tab always lines the tooltip up with
				// the actual visible pill (not the old hardcoded 7 px
				// / 42 px assumption).
				const float t_item_y = nav_rail_start_y
					+ hovered_main_tab * nav_rail_item_spacing;
				const float tooltip_x = nav_rail_x - tab.tooltip_w - 8.0f;
				const float tooltip_y = t_item_y + 5.0f;
				bypass::overlay::filled_rounded_rectangle(
					{ tooltip_x, tooltip_y, tab.tooltip_w, 26.0f },
					7.0f, bypass::overlay::color_t { 24, 22, 39, 248 } );
				bypass::overlay::rounded_rectangle(
					{ tooltip_x, tooltip_y, tab.tooltip_w, 26.0f },
					7.0f, col_border, 1.0f );
				bypass::overlay::text_ex(
					tab.label,
					{ tooltip_x, tooltip_y, tab.tooltip_w, 26.0f },
					bypass::overlay::color_t { 232, 229, 240, 220 },
					12.0f, bypass::overlay::text_weight_t::semi_bold,
					L"Inter-Medium", bypass::overlay::text_align_t::center, false );
			}

			// Panel outline (only when panel is visible)
			bypass::overlay::rounded_rectangle( panel, panel_radius, { 66, 66, 66, 130 }, 1.5f );
		}
		// ================================================================
		//  END PANEL
		// ================================================================

#if 0
		// ---- Watermark (ALWAYS drawn) ----------------------------------------
		constexpr float watermark_x = 18.0f;
		constexpr float watermark_y = 18.0f;
		const float watermark_role_w = ( std::max ) (
			62.0f, static_cast< float >( s_profile_role.size( ) ) * 7.5f );
		const float watermark_content_w = 48.0f + 16.0f + watermark_role_w + 16.0f + 62.0f + 16.0f + 56.0f;
		const float watermark_w = ( std::max ) ( 258.0f, watermark_content_w + 36.0f );
		constexpr float watermark_h = 48.0f;

		static float watermark_fps = 60.0f;
		static float watermark_ops = 0.0f;
		static std::uint32_t watermark_frames = 0;
		static auto watermark_sample_start = animation_now;
		++watermark_frames;
		const float watermark_sample_seconds = std::chrono::duration<float>(
			animation_now - watermark_sample_start ).count( );
		if ( watermark_sample_seconds >= 0.5f ) {
			watermark_fps = watermark_frames / watermark_sample_seconds;
			const auto raw_ops = bypass::g_ops_counter.exchange( 0, std::memory_order_relaxed );
			watermark_ops = static_cast< float >( raw_ops ) / watermark_sample_seconds;
			watermark_frames = 0;
			watermark_sample_start = animation_now;
		}
		wchar_t watermark_fps_text[ 24 ] {};
		std::swprintf( watermark_fps_text, std::size( watermark_fps_text ), L"%.0f fps", watermark_fps );
		wchar_t watermark_ops_text[ 24 ] {};
		if ( watermark_ops >= 1'000'000.0f )
			std::swprintf( watermark_ops_text, std::size( watermark_ops_text ), L"%.1fM/s", watermark_ops / 1'000'000.0f );
		else if ( watermark_ops >= 1'000.0f )
			std::swprintf( watermark_ops_text, std::size( watermark_ops_text ), L"%.1fk/s", watermark_ops / 1'000.0f );
		else
			std::swprintf( watermark_ops_text, std::size( watermark_ops_text ), L"%.0f/s", watermark_ops );

		bypass::overlay::filled_rounded_rectangle(
			{ watermark_x + 2.0f, watermark_y + 4.0f, watermark_w, watermark_h }, 14.0f,
			bypass::overlay::color_t { 0, 0, 0, 95 } );
		bypass::overlay::filled_rounded_rectangle(
			{ watermark_x, watermark_y, watermark_w, watermark_h }, 14.0f,
			bypass::overlay::color_t { 15, 13, 26, 248 } );
		bypass::overlay::filled_rounded_rectangle(
			{ watermark_x, watermark_y, watermark_w, watermark_h }, 14.0f,
			bypass::overlay::color_t { 32, 29, 42, 44 } );
		bypass::overlay::rounded_rectangle(
			{ watermark_x, watermark_y, watermark_w, watermark_h }, 14.0f,
			bypass::overlay::color_t { 62, 66, 84, 145 }, 1.0f );

		float watermark_text_x = watermark_x + ( watermark_w - watermark_content_w ) * 0.5f;
		bypass::overlay::text_ex( L"crisis",
			{ watermark_text_x, watermark_y, 48.0f, watermark_h },
			col_text_active, 14.0f, bypass::overlay::text_weight_t::bold,
			L"Geist-Bold", bypass::overlay::text_align_t::center, false );
		watermark_text_x += 48.0f;
		bypass::overlay::text_ex( L"|",
			{ watermark_text_x, watermark_y, 16.0f, watermark_h },
			bypass::overlay::color_t { 210, 205, 220, 80 }, 12.0f,
			bypass::overlay::text_weight_t::regular,
			L"Inter-Regular", bypass::overlay::text_align_t::center, false );
		watermark_text_x += 16.0f;
		bypass::overlay::text_ex( s_profile_role.c_str( ),
			{ watermark_text_x, watermark_y, watermark_role_w, watermark_h },
			bypass::overlay::color_t { 239, 236, 246, 225 }, 12.5f,
			bypass::overlay::text_weight_t::semi_bold,
			L"Inter-Medium", bypass::overlay::text_align_t::center, false );
		watermark_text_x += watermark_role_w;
		bypass::overlay::text_ex( L"|",
			{ watermark_text_x, watermark_y, 16.0f, watermark_h },
			bypass::overlay::color_t { 210, 205, 220, 80 }, 12.0f,
			bypass::overlay::text_weight_t::regular,
			L"Inter-Regular", bypass::overlay::text_align_t::center, false );
		watermark_text_x += 16.0f;
		bypass::overlay::text_ex( watermark_fps_text,
			{ watermark_text_x, watermark_y, 62.0f, watermark_h },
			col_text_active, 12.5f, bypass::overlay::text_weight_t::semi_bold,
			L"Inter-Medium", bypass::overlay::text_align_t::center, false );
		watermark_text_x += 62.0f;
		bypass::overlay::text_ex( L"|",
			{ watermark_text_x, watermark_y, 16.0f, watermark_h },
			bypass::overlay::color_t { 210, 205, 220, 80 }, 12.0f,
			bypass::overlay::text_weight_t::regular,
			L"Inter-Regular", bypass::overlay::text_align_t::center, false );
		watermark_text_x += 16.0f;
		bypass::overlay::text_ex( watermark_ops_text,
			{ watermark_text_x, watermark_y, 56.0f, watermark_h },
			bypass::overlay::color_t { 180, 176, 210, 210 }, 12.5f,
			bypass::overlay::text_weight_t::semi_bold,
			L"Inter-Medium", bypass::overlay::text_align_t::center, false );
		bypass::overlay::filled_rounded_rectangle(
			{ watermark_x + 24.0f, watermark_y + watermark_h - 3.0f,
			  watermark_w - 48.0f, 1.0f }, 0.5f,
			bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 75 } );
#endif

		// ---- Notifications (ALWAYS drawn) ------------------------------------
		constexpr float notification_w = 344.0f;
		constexpr float notification_h = 56.0f;
		constexpr float notification_gap = 10.0f;
		constexpr float notification_margin = 18.0f;
		constexpr float notification_enter = 0.34f;
		constexpr float notification_hold = 3.4f;
		constexpr float notification_exit = 0.28f;
		const float notification_lifetime = notification_enter + notification_hold + notification_exit;

		// Ensure screen width is valid so it doesn't draw at negative/offscreen X
		if ( gui::screen_width <= 0 || gui::screen_height <= 0 ) {
			bypass::overlay::get_size( gui::screen_width, gui::screen_height );
			if ( gui::screen_width <= 0 ) gui::screen_width = 1920;
			if ( gui::screen_height <= 0 ) gui::screen_height = 1080;
		}

		std::lock_guard notifications_lock( notifications::queue_mutex );
		for ( auto& notification : notifications::queue ) {
			if ( !notification.pinned || notification.age < 0.34f )
				notification.age += animation_delta;
		}

		notifications::queue.erase( std::remove_if(
			notifications::queue.begin( ), notifications::queue.end( ),
			[ & ]( const notifications::item_t& notification ) {
				return notification.age >= notification_lifetime;
			} ), notifications::queue.end( ) );

		for ( std::size_t i = 0; i < notifications::queue.size( ); ++i ) {
			auto& notification = notifications::queue[ i ];
			const float target_y = notification_margin + i * ( notification_h + notification_gap );
			if ( notification.current_y < 0.0f )
				notification.current_y = target_y;

			const float stack_ease = 1.0f - std::exp( -animation_delta * 14.0f );
			notification.current_y += ( target_y - notification.current_y ) * stack_ease;

			// Bulletproof linear fade in/out
			float visibility = 1.0f;
			if ( notification.age < notification_enter ) {
				visibility = notification.age / notification_enter;
			}
			else if ( notification.age > notification_enter + notification_hold ) {
				const float t = ( std::clamp ) (
					( notification.age - notification_enter - notification_hold )
					/ notification_exit, 0.0f, 1.0f );
				visibility = 1.0f - t;
			}

			const float target_x = static_cast< float >( gui::screen_width )
				- notification_margin - notification_w;

			// Slide in from just 50px to the right (instead of fully offscreen)
			const float slide_t = ( std::clamp ) ( notification.age / notification_enter, 0.0f, 1.0f );
			const float slide_ease = 1.0f - ( 1.0f - slide_t ) * ( 1.0f - slide_t );
			const float x = target_x + ( 1.0f - slide_ease ) * 50.0f;
			const float y = notification.current_y;

			// Pick the accent colour by default; when the pusher
			// supplied a real colour (any non-zero RGB), use that
			// instead — this is what carries semantic tone through
			// the whole notification card (border, backdrop dots,
			// icon box, and the icon glyph itself below).
			const bool has_custom_color = notification.color.r != 0
				|| notification.color.g != 0
				|| notification.color.b != 0;
			const bypass::overlay::color_t notification_color = has_custom_color
				? notification.color
				: col_accent;

			auto alpha = [ & ]( float base ) {
				return static_cast< std::uint8_t >( ( std::clamp ) ( base * visibility, 0.0f, 255.0f ) );
				};

			// 1. Base & Tinted Glass Background
			bypass::overlay::filled_rounded_rectangle( { x, y, notification_w, notification_h }, 12.0f, bypass::overlay::color_t { col_bg.r, col_bg.g, col_bg.b, alpha( col_bg.a ) } );
			bypass::overlay::filled_rounded_rectangle( { x, y, notification_w, notification_h }, 12.0f, bypass::overlay::color_t { 28, 25, 40, alpha( 220 ) } );

			// 2. Dot Pattern
			constexpr float dot_spacing = 20.0f;
			constexpr float outer_dot_radius = 2.5f;
			const bypass::overlay::point_t pattern_center {
				notification_w * 0.5f, notification_h * 0.5f };
			const float maximum_distance = std::sqrt(
				pattern_center.x * pattern_center.x + pattern_center.y * pattern_center.y );
			const float notification_animation_time = std::chrono::duration< float >(
				animation_now.time_since_epoch( ) ).count( );

			bypass::overlay::push_clip( { x, y, notification_w, notification_h } );
			for ( float dot_y = 0.0f; dot_y < notification_h; dot_y += dot_spacing ) {
				for ( float dot_x = 0.0f; dot_x < notification_w; dot_x += dot_spacing ) {
					const float dx = dot_x - pattern_center.x;
					const float dy = dot_y - pattern_center.y;
					const float distance = std::sqrt( dx * dx + dy * dy );
					const float distance_fade = ( std::max ) (
						0.0f, 1.0f - distance / maximum_distance );
					const float pulse = 0.5f + 0.5f * std::sin(
						notification_animation_time * 2.0f + ( dot_x + dot_y ) * 0.05f );

					const float dot_opacity = 0.85f * distance_fade * pulse;
					if ( dot_opacity <= 0.05f ) continue;

					const auto dot_alpha = alpha( dot_opacity * 255.0f );
					const bypass::overlay::point_t dot { x + dot_x + 1.0f, y + dot_y + 1.0f };

					bypass::overlay::filled_circle( dot, outer_dot_radius,
						bypass::overlay::color_t { notification_color.r, notification_color.g,
							notification_color.b, dot_alpha } );
				}
			}
			bypass::overlay::pop_clip( );

			// --- FIX APPLIED HERE: Outer border now uses the dynamic notification_color ---
			bypass::overlay::rounded_rectangle(
				{ x, y, notification_w, notification_h }, 12.0f,
				bypass::overlay::color_t {
					notification_color.r, notification_color.g, notification_color.b,
					alpha( 150.0f )
				}, 2.5f );

			// 3. Accent Border & Icon Box
			bypass::overlay::filled_rounded_rectangle(
				{ x + 11.0f, y + 8.0f, 50.0f, 40.0f }, 11.0f,
				bypass::overlay::color_t { notification_color.r, notification_color.g,
					notification_color.b, alpha( 70.0f ) } );
			bypass::overlay::rounded_rectangle(
				{ x + 11.0f, y + 8.0f, 50.0f, 40.0f }, 11.0f,
				bypass::overlay::color_t { notification_color.r, notification_color.g,
					notification_color.b, alpha( 200.0f ) }, 1.0f );

			// Icon tint tracks the same notification_color the card
			// border/backdrop use — green success / red failure /
			// accent info now read at a glance instead of a uniform
			// white glyph. notification_color falls back to accent
			// when the caller pushed no colour.
			bypass::overlay::text_ex( notification.icon.c_str( ),
				{ x + 11.0f, y + 8.0f, 50.0f, 40.0f },
				bypass::overlay::color_t { notification_color.r,
					notification_color.g, notification_color.b,
					alpha( 255.0f ) },
				18.0f, bypass::overlay::text_weight_t::regular, L"Font Awesome",
				bypass::overlay::text_align_t::center, false );

			// 4. Text
			bypass::overlay::text_ex( notification.header.c_str( ),
				{ x + 73.0f, y + 8.0f, notification_w - 86.0f, 20.0f },
				bypass::overlay::color_t { 255, 255, 255, alpha( 255.0f ) }, 12.5f,
				bypass::overlay::text_weight_t::bold, L"Geist-Bold",
				bypass::overlay::text_align_t::left, false );
			bypass::overlay::text_ex( notification.description.c_str( ),
				{ x + 73.0f, y + 25.0f, notification_w - 86.0f, 20.0f },
				bypass::overlay::color_t { 210, 205, 225, alpha( 235.0f ) }, 11.0f,
				bypass::overlay::text_weight_t::bold, L"Inter-Regular",
				bypass::overlay::text_align_t::left, false );
		}

		// ── Widget layout overlay ────────────────────────────
		// Rendered LAST so it draws over every menu / HUD /
		// notification pass. Framework-only for now — the
		// function body's TODO(widget-layout) sections describe
		// what still needs to be drawn. When you're ready to
		// implement, just fill in those sections; the sidebar
		// toggle, registry, and dynamic-spacing math are already
		// wired end-to-end so no additional plumbing is needed.
		if ( s_widget_layout_mode ) {
			render_widget_layout_overlay( mx, my, lmb, clicked,
				col_accent, col_border );
		}
	}
}
