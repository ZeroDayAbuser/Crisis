#pragma once

namespace engine {
	enum class e_fort_rarity : std::uint8_t {
		common = 0,
		uncommon = 1,
		rare = 2,
		epic = 3,
		legendary = 4,
		mythic = 5,
		transcendent = 6,
		unattainable = 7,
		num_rarity_values = 8
	};

	enum class building_style : std::uint8_t {
		building = 0,
		editing = 1,
		none = 2
	};

	enum class e_collision_channel : uint8_t {
		world_static = 0,
		world_dynamic = 1,
		pawn = 2,
		visibility = 3,
		camera = 4,
		physics_body = 5,
		vehicle = 6,
		destructible = 7,
		engine_trace_1 = 8,
		engine_trace_2 = 9,
		engine_trace_3 = 10,
		engine_trace_4 = 11,
		engine_trace_5 = 12,
		engine_trace_6 = 13,
		game_trace_1 = 14,
		game_trace_2 = 15,
		game_trace_3 = 16,
		game_trace_4 = 17,
		game_trace_5 = 18,
		game_trace_6 = 19,
		game_trace_7 = 20,
		game_trace_8 = 21,
		game_trace_9 = 22,
		game_trace_10 = 23,
		game_trace_11 = 24,
		game_trace_12 = 25,
		game_trace_13 = 26,
		game_trace_14 = 27,
		game_trace_15 = 28,
		game_trace_16 = 29,
		game_trace_17 = 30,
		game_trace_18 = 31,
		overlap_all_deprecated = 32
	};

	enum class e_weapon_core_animation : std::uint8_t {
		melee = 0,
		pistol = 1,
		shotgun = 2,
		paper_blueprint = 3,
		rifle = 4,
		melee_one_hand = 5,
		machine_pistol = 6,
		rocket_launcher = 7,
		grenade_launcher = 8,
		going_commando = 9,
		assault_rifle = 10,
		tactical_shotgun = 11,
		sniper_rifle = 12,
		trap_placement = 13,
		shoulder_launcher = 14,
		ability_deco_tool = 15,
		crossbow = 16,
		c4 = 17,
		remote_control = 18,
		dual_wield = 19,
		ar_bullpup = 20,
		ar_forward_grip = 21,
		med_pack_paddles = 22,
		smg_p90 = 23,
		ar_drum_gun = 24,
		consumable_small = 25,
		consumable_large = 26,
		balloon = 27,
		mounted_turret = 28,
		creative_tool = 29,
		explosive_bow = 30,
		ashton_indigo = 31,
		ashton_chicago = 32,
		melee_dual_wield = 33,
		unarmed = 34,
		max = 35
	};

	static bool is_unarmed( e_weapon_core_animation anim ) {
		return anim == e_weapon_core_animation::melee ||
			anim == e_weapon_core_animation::unarmed ||
			anim == e_weapon_core_animation::melee_one_hand ?
			true : false;
	}
}