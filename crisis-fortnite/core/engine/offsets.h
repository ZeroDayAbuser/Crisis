#pragma once

namespace engine {
	namespace offsets {
		constexpr std::uint32_t world = 0x78;
		constexpr std::uint32_t viewport_client = 0xb70;

		constexpr std::uint32_t gengine = 0x1B2C7518;
		constexpr std::uint32_t gnames = 0x1B179F00;

		constexpr std::uint32_t gobjects = 0x1A6CBFA0;

		constexpr std::uint32_t owning_game_instance = 0x238; // OwningGameInstance
		constexpr std::uint32_t local_players = 0x38;
		constexpr std::uint32_t levels = owning_game_instance - 0x60;
		constexpr std::uint32_t current_gun_fire_index = 0xf30; // CurrentGunFireIndex
		constexpr std::uint32_t persistent_level = 0x38; // PersistentLevel
		constexpr std::uint32_t actor_cluster = 0x1A0;
		constexpr std::uint32_t player_array = 0x288;
		constexpr std::uint32_t player_controller = 0x30;
		constexpr std::uint32_t player_state = 0x290;
		constexpr std::uint32_t acknowledged_pawn = 0x318;
		constexpr std::uint32_t pawn_private = 0x2e8;
		constexpr std::uint32_t game_state = 0x1C0;
		constexpr std::uint32_t mesh = 0x2f0;
		constexpr std::uint32_t revive_from_dbno_time = 0x4ad8; // ReviveFromDBNOTime
		constexpr std::uint32_t component_to_world = 0x1e0;
		constexpr std::uint32_t bone_array = 0x660;
		constexpr std::uint32_t bone_array_cache = 0x660;
		constexpr std::uint32_t component_space_transforms_array = 0x660;
		constexpr std::uint32_t cached_bone_space_transforms = 0x9b0;
		constexpr std::uint32_t cached_component_space_transforms = 0x9c0;
		constexpr std::uint32_t default_physics_volume = 0x158; // DefaultPhysicsVolume
		constexpr std::uint32_t current_read_component_transforms = 0x68;
		constexpr std::uint32_t cached_view_info_rendered_last_frame = default_physics_volume + 0x20;
		constexpr std::uint32_t player_name = 0x9E8;
		constexpr std::uint32_t player_name_private = 0x308;
		constexpr std::uint32_t item_rarity = 0xaa;
		constexpr std::uint32_t item_tier = 0xaa;
		constexpr std::uint32_t rarity = 0x1873E5D8;
		constexpr std::uint32_t item_name = 0x38;
		constexpr std::uint32_t ftext_data = 0x20;
		constexpr std::uint32_t ftext_length = 0x28;
		constexpr std::uint32_t weapon_data = 0x628;
		constexpr std::uint32_t cosmetic_override_weapon_data = weapon_data + 8;
		constexpr std::uint32_t ammo_count = 0x1100;
		constexpr std::uint32_t current_weapon = 0x9d0;
		constexpr std::uint32_t b_is_reloading_weapon = 0x371; // bIsReloadingWeapon
		constexpr std::uint32_t b_is_dbno = 0x881;
		constexpr std::uint8_t  b_is_dbno_bit = 7;
		constexpr std::uint32_t b_is_dying = 0x728;
		constexpr std::uint8_t  b_is_dying_bit = 5;
		constexpr std::uint32_t b_is_a_bot = 0x27a;
		constexpr std::uint8_t  b_is_a_bot_bit = 3;
		constexpr std::uint32_t team_index = 0xf69;
		constexpr std::uint32_t kill_score = 0xf80;
		constexpr std::uint32_t season_level_ui_display = kill_score + 4;
		constexpr std::uint32_t ranked_progress = 0xD8;  // FRankedProgressReplicatedData struct base on habanero
		constexpr std::uint32_t rank_progress = 0xE8;  // Rank       int32 (habanero+D8 + FString16 prefix)
		constexpr std::uint32_t highest_rank = 0xEC;  // HighestRank int32
		constexpr std::uint32_t current_mmr = 0xF0;  // CurrentMMR  int32
		constexpr std::uint32_t current_position_rank = 0xF4;  // CurrentPosition int32
		constexpr std::uint32_t habanero_component = 0x918;
		constexpr std::uint32_t habanero_component_controller = 0x4510;
		constexpr std::uint32_t building_state = 0x2090;
		constexpr std::uint32_t initial_squad_size = 0x1728;
		constexpr std::uint32_t squad_size_decrements = initial_squad_size + 0x2;
		constexpr std::uint32_t b_is_in_safe_zone = 0x26b8; // bIsInsideSafeZone
		constexpr std::uint32_t b_is_skydiving = 0x228f;
		constexpr std::uint32_t platform = 0x400;
		constexpr std::uint32_t client_settings_record = 0x310;
		constexpr std::uint32_t mouse_sensitivity = 0x750;
		constexpr std::uint32_t simulating_too_long_length = 0x290;
		constexpr std::uint32_t max_level = 0x2ac;
		constexpr std::uint32_t search_text = 0xd38;
		constexpr std::uint32_t root_component = 0x1b0;
		constexpr std::uint32_t relative_location = 0x140;
		constexpr std::uint32_t relative_rotation = 0x158;
		constexpr std::uint32_t relative_scale_3d = 0x170;
		constexpr std::uint32_t component_velocity = 0x188;

		constexpr std::uint32_t location_pointer = 0x168;
		constexpr std::uint32_t rotation_pointer = location_pointer + 0x10;

		constexpr std::uint32_t camera_fov = 0x374;
		constexpr std::uint32_t player_camera_manager = 0x328;
		constexpr std::uint32_t default_fov = 0x284;

		constexpr std::uint32_t fov_min = 0x2258;
		constexpr std::uint32_t fov_max = fov_min + 4;

		constexpr std::uint32_t targeted_fort_pawn = 0x16c0;
		constexpr std::uint32_t location_under_reticle = 0x21a0;
		constexpr std::uint32_t my_fort_pawn = 0xaf8;

		constexpr std::uint32_t aim_assist_offset = 0x22e0;
		constexpr std::uint32_t additional_aim_offset = aim_assist_offset + 0x20;

		constexpr std::uint32_t last_render_time = 0x290; // 0x7e4; // 0x2E0 i think idk
		constexpr std::uint32_t last_submit_time = 0x530;
		constexpr std::uint32_t server_world_time = 0x2a8;

		constexpr std::uint32_t spectators = 0xac0;
		constexpr std::uint32_t spectator_array = 0x48;

		constexpr std::uint32_t current_vehicle = 0x2c30;
		constexpr std::uint32_t health = 0xd0c;
		constexpr std::uint32_t max_health = health + 0x4;

		constexpr std::uint32_t last_reload_time = 0x1090;
		// Fixed from the paste's self-referencing initializer
		// (current_reload_duration = current_reload_duration + 0x8) which
		// wouldn't compile. Sits 0x8 past last_reload_time as the paste's
		// naming (last_reload_time / current_reload_duration) implies.
		constexpr std::uint32_t current_reload_duration = last_reload_time + 0x8;
		constexpr std::uint32_t last_fire_time = 0x1004;
		constexpr std::uint32_t global_anim_rate_scale = 0xa18;

		constexpr std::uintptr_t
			building_static_mesh = 0x7a8,
			static_mesh_component = 0x2f0,
			static_mesh_component_static_mesh = 0x5e8,
			skinned_mesh_component_skeletal_mesh = 0x5e8,
			skinned_mesh_component_override_materials = 0x570,
			landscape_nanite_component = 0x3D0,
			static_mesh_body_setup = 0x168,
			body_setup_agg_geom = 0x30,
			agg_geom_box_elems = 0x10,
			agg_geom_convex_elems = 0x30;

		constexpr std::uint32_t
			weapon_muzzle_socket_name = 0x1390,
			weapon_all_weapon_meshes = 0xF98,
			mesh_skeletal_mesh = 0x5d0,
			mesh_current_read_component_transforms = 0x68,
			skeletal_mesh_skeleton = 0xD0,
			skeletal_mesh_bone_info_array = 0x280,
			skeletal_mesh_sockets = 0x398,
			skeleton_sockets = 0x128,
			socket_socket_name = 0x28,
			socket_bone_name = 0x2C,
			socket_relative_location = 0x30,
			socket_relative_rotation = 0x48;

		constexpr std::uint32_t last_fired_location = 0x5670;
		constexpr std::uint32_t last_fired_direction = last_fired_location + 0x18;
		constexpr std::uint32_t last_fired_time = last_fired_direction + 0x18;
		constexpr std::uint32_t current_projected_impact_distance = 0xfe0;

		constexpr std::uint32_t skeletal_meshes = last_fired_time + 0x6e0;

		constexpr std::uint32_t dbno_revive_time = 0x4ad8; // ReviveFromDBNOTime
		constexpr std::uint32_t dbno_start_time = 0x4ae0; // DBNOStartTime
		constexpr std::uint32_t actor_death_time = 0x49b0;
		constexpr std::uint32_t last_hit_time = 0x14d0;
		constexpr std::uint32_t last_damaged_time = 0xde8;   // LastDamagedTime — advances every time the actor takes damage
		constexpr std::uint32_t ability_system_component = 0x1430;
		constexpr std::uint32_t game_elapsed_time = 0x02D0;
		constexpr std::uint32_t players_left = 0x1620;
		constexpr std::uint32_t teams_left = 0x1700;
		constexpr std::uint32_t winning_player_state = 0x17a8;  // APlayerState* — null until the game ends
		constexpr std::uint32_t winning_team = winning_player_state + 0x18;  // int           — 0 until the game ends
		constexpr std::uint32_t weapon_core_animation = 0x16d0;

		constexpr std::uint32_t primary_pickup_item_entry = 0x368;
		constexpr std::uint32_t item_definition = 0x10;

		// ── ULocalPlayer view state ──────────────────────────────────────────
		constexpr std::uint32_t view_states = 0xD0;  // ULocalPlayer::ViewStates — TArray<FSceneViewStateReference>

		// ── FSceneViewState ───────────────────────────────────────────────────
		constexpr std::uint32_t cached_view_matrices_projection_data = 0x530;  // FSceneViewState::CachedViewMatricesProjectionData → FViewMatrices

		// ── UE-standard SDK sub-offsets (preserved) ──────────────────────────
		// Not part of the recent paste but still consumed by classes.h's
		// declare_member bindings that power the magic_bullet exploit
		// (u_object::class_private → u_class::default_object → mesh →
		// skinned_asset → skeletal_mesh::final_ref_bone_pose). Values are
		// UE5-standard and unchanged across recent Fortnite builds; update
		// only if a future dump moves them.
		constexpr std::uint32_t class_private = 0x20;
		constexpr std::uint32_t class_default_object = 0xC8;
		constexpr std::uint32_t skinned_asset = 0x5F0;
		constexpr std::uint32_t skeletal_mesh_final_ref_bone_pose = 0x268 + 0x48;

		constexpr std::uint32_t static_find_object = 0x825BF4;
		constexpr std::uint32_t static_load_object = 0x5B6D05;

		constexpr std::uint32_t add_to_root = 0xBEEF/*BURGER*/;

		constexpr std::uint32_t process_event = 0xA9D7C;

		constexpr std::uint32_t to_string = 0x38efc;
		constexpr std::uint32_t to_string_state = 0x1A33C900;
		constexpr std::uint32_t to_string_state_guard_delta = 0x18;
		constexpr std::uint32_t to_string_state_chunk_array = 0x6768;
		constexpr std::uint32_t to_string_state_map_size = 0x10000;
		constexpr std::uint32_t fname_pool = 0x1A5EF440;
		constexpr std::uint32_t fname_pool_chunk_array = 0x1ABD7500u + 0x1C28u;

		// ── Exploits · No-spread (reticle bloom) ────────────────────────────
		// Weapon-relative offsets to the obfuscated spread/reticle index
		// tokens walked by the zero-reticle exploit in cache/local.cxx.
		// Each entry names a distinct spread state on the weapon; the
		// walker runs every entry each iteration so whichever slot the
		// active weapon actually uses is covered without a per-weapon
		// classification pass.
		//
		// Slot layout at each offset (little-endian):
		//   [ off - 0x10 .. off ] : 4x uint32 encrypted value slots
		//   [ off              ] : uint8 obfuscated selector — after
		//                          (raw ^ 0x39) & 3 it indexes
		//                          aim_idx_table to pick which of the
		//                          4 value slots is live this frame.
		//   [ off + 4          ] : uint32 counter used to derive the
		//                          XOR key for the active slot.
		constexpr std::uint16_t spread_offset[ 6 ] = {
			0x2548, 0x2520, 0x2618, 0x24F0, 0x2508, 0x2600,
		};

		// Permutation applied to the deobfuscated selector before it
		// indexes the 4-slot value bank. Identity default matches the
		// current build; if a future build shuffles the bank layout
		// only this table needs updating — the walker is unaffected.
		constexpr std::uint8_t aim_idx_table[ 4 ] = { 0, 1, 2, 3 };
	}

	enum skeleton {
		root = 0,
		attach = 1,
		pelvis = 2,
		spine_01 = 3,
		spine_02 = 4,
		spine_03 = 5,
		spine_04 = 6,
		spine_05 = 7,
		clavicle_l = 8,
		upperarm_l = 9,
		lowerarm_l = 10,
		hand_l = 11,
		index_metacarpal_l = 12,
		index_01_l = 13,
		index_02_l = 14,
		index_03_l = 15,
		middle_metacarpal_l = 16,
		middle_01_l = 17,
		middle_02_l = 18,
		middle_03_l = 19,
		pinky_metacarpal_l = 20,
		pinky_01_l = 21,
		pinky_02_l = 22,
		pinky_03_l = 23,
		ring_metacarpal_l = 24,
		ring_01_l = 25,
		ring_02_l = 26,
		ring_03_l = 27,
		thumb_01_l = 28,
		thumb_02_l = 29,
		thumb_03_l = 30,
		weapon_l = 31,
		hand_attach_l = 32,
		lowerarm_twist_01_l = 33,
		lowerarm_twist_02_l = 34,
		upperarm_twist_01_l = 35,
		upperarm_twist_02_l = 36,
		clavicle_r = 37,
		upperarm_r = 38,
		lowerarm_r = 39,
		hand_r = 40,
		index_metacarpal_r = 41,
		index_01_r = 42,
		index_02_r = 43,
		index_03_r = 44,
		middle_metacarpal_r = 45,
		middle_01_r = 46,
		middle_02_r = 47,
		middle_03_r = 48,
		pinky_metacarpal_r = 49,
		pinky_01_r = 50,
		pinky_02_r = 51,
		pinky_03_r = 52,
		ring_metacarpal_r = 53,
		ring_01_r = 54,
		ring_02_r = 55,
		ring_03_r = 56,
		thumb_01_r = 57,
		thumb_02_r = 58,
		thumb_03_r = 59,
		weapon_r = 60,
		hand_attach_r = 61,
		lowerarm_twist_01_r = 62,
		lowerarm_twist_02_r = 63,
		upperarm_twist_01_r = 64,
		upperarm_twist_02_r = 65,
		neck_01 = 66,
		neck_02 = 67,
		chest = 68,
		head = 110,
		attach_backpack = 69,
		attach_cape = 70,
		thigh_l = 71,
		calf_l = 72,
		calf_twist_01_l = 73,
		calf_twist_02_l = 74,
		foot_l = 86,
		ball_l = 76,
		thigh_twist_01_l = 77,
		thigh_r = 78,
		calf_r = 79,
		calf_twist_01_r = 80,
		calf_twist_02_r = 81,
		foot_r = 87,
		ball_r = 83,
		thigh_twist_01_r = 84,
		ik_foot_root = 85,
		ik_foot_l = 86,
		ik_foot_r = 87,
		ik_hand_root = 88,
		ik_hand_gun = 89,
		ik_hand_l = 90,
		ik_hand_r = 91
	};
}
