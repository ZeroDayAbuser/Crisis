#include <deps/includes.h>

namespace gui {
    namespace player {
        color_t team_color( std::uint8_t idx ) {
            // Golden angle (≈137.5°) distributes hues with maximum perceptual separation
            const float hue = std::fmod( static_cast<float>( idx ) * 137.508f, 360.f );
            const float s = 0.80f, v = 0.95f;
            const float h6 = hue / 60.f;
            const int   i  = static_cast<int>( h6 ) % 6;
            const float f  = h6 - static_cast<float>( i );
            const float p  = v * ( 1.f - s );
            const float q  = v * ( 1.f - s * f );
            const float t  = v * ( 1.f - s * ( 1.f - f ) );
            float r, g, b;
            switch ( i ) {
            case 0: r=v; g=t; b=p; break;
            case 1: r=q; g=v; b=p; break;
            case 2: r=p; g=v; b=t; break;
            case 3: r=p; g=q; b=v; break;
            case 4: r=t; g=p; b=v; break;
            default: r=v; g=p; b=q; break;
            }
            return {
                static_cast<std::uint8_t>( r * 255.f ),
                static_cast<std::uint8_t>( g * 255.f ),
                static_cast<std::uint8_t>( b * 255.f ),
                255
            };
        }

		void render(
			cache::c_player_pawn& player_pawn,
            const std::shared_ptr< player_stats::player_stats_cache_t >& stats_cache ) {
            if ( player_pawn.m_is_dying )
                return;

            // Slot ordering — populate per-player (cheap small-N sort).
            // begin_frame reads config::player::stats::text_anchors /
            // text_orders + the individual toggles to build the display-
            // position table that text_*_slot() below reads.
            esp::begin_frame( );

            const std::uint8_t local_team_index = g_cache->m_local_team_index.load( std::memory_order_acquire );
            const auto is_visible  = player_pawn.m_is_visible;
            const bool is_teammate = local_team_index != 0 && player_pawn.m_team_index == local_team_index;
            const auto is_robot    = player_pawn.m_player_name.find( static_cast<const char*>( obf( "Robot" ) ) ) != std::string::npos;

			if ( config::aimbot::enabled && !is_teammate &&
				( config::aimbot::aim_at_npcs || !is_robot ) &&
				( config::aimbot::aim_at_knocked || !player_pawn.m_is_downed ) &&
				g_aimbot->is_valid( player_pawn ) )
				g_aimbot->add( player_pawn );

            const auto& origin = player_pawn.m_bounds.orgin;
            const auto& extent = player_pawn.m_bounds.box_extent;
			const auto& head_screen = world_to_screen( player_pawn.m_head_location );
			const auto& distance = cache::camera_location.distance_to( player_pawn.m_root_location );

            engine::f_vector2d min_pos( DBL_MAX, DBL_MAX );
            engine::f_vector2d max_pos( -DBL_MAX, -DBL_MAX );

            bool any_corner = false;
            for ( int i = 0; i < 8; i++ ) {
                const auto corner = origin + engine::f_vector(
                    ( i & 1 ) ? extent.x : -extent.x,
                    ( i & 2 ) ? extent.y : -extent.y,
                    ( i & 4 ) ? extent.z : -extent.z
                );

                const auto corner_screen = world_to_screen( corner );
                if ( !corner_screen )
                    continue;

                any_corner = true;
                min_pos.x = corner_screen->x < min_pos.x ? corner_screen->x : min_pos.x;
                min_pos.y = corner_screen->y < min_pos.y ? corner_screen->y : min_pos.y;
                max_pos.x = corner_screen->x > max_pos.x ? corner_screen->x : max_pos.x;
                max_pos.y = corner_screen->y > max_pos.y ? corner_screen->y : max_pos.y;
            }

			const bool is_on_screen = any_corner &&
				max_pos.x > min_pos.x && max_pos.y > min_pos.y &&
				max_pos.x >= 0.0 && min_pos.x <= static_cast< double >( gui::screen_width ) &&
				max_pos.y >= 0.0 && min_pos.y <= static_cast< double >( gui::screen_height );

            constexpr float width_multiplier = 1.09f;
            constexpr float height_multiplier = 1.02f;

            const float center_x = static_cast< float >( ( min_pos.x + max_pos.x ) * 0.5 );
            const float center_y = static_cast< float >( ( min_pos.y + max_pos.y ) * 0.5 );
            const float half_width = static_cast< float >( ( max_pos.x - min_pos.x ) * 0.5 ) * width_multiplier;
            const float half_height = static_cast< float >( ( max_pos.y - min_pos.y ) * 0.5 ) * height_multiplier;

            constexpr color_t orange_color{ 255, 177, 109, 255 };
            constexpr color_t outline_color{ 0,   0,   0, 255 };

			const color_t col_accent = menu::current_accent( );
			const color_t col_passive = menu::inactive_accent( );
            const color_t col_primary = is_teammate ? orange_color : ( ( is_visible && is_on_screen ) ? col_accent : col_passive );
			const color_t col_pastel = is_teammate ? orange_color
				: ( ( is_visible && is_on_screen ) ? menu::pastel_accent( col_accent ) : menu::pastel_accent( col_passive ) );

			if ( config::player::tracers::enabled ) {
				const auto tracer_key = reinterpret_cast< std::uint64_t >( player_pawn.m_mesh );
				esp::tracers::sample( tracer_key, player_pawn.m_root_location );
				esp::tracers::render_player_trail( tracer_key, col_pastel );
			}

            esp::layout_t layout{};
            layout.box = rect_t{
                center_x - half_width,
                center_y - half_height,
                half_width * 2.f,
                half_height * 2.f
            };

            //if ( config::player::chams::enabled ) {
            //    static std::unordered_map<
            //        std::uintptr_t,
            //        std::vector< std::pair< std::int32_t, engine::u_material_interface* > >
            //    > s_backups;

            //    const auto material = g_cache->m_material.load( );
            //    if ( material && utility::is_valid_pointer( material ) ) {
            //        auto skeletal_meshes = player_pawn.m_skeletal_meshes;
            //        //logging::print( obf( "chams: found %d skeletal mesh(es)" ), skeletal_meshes.count );

            //        for ( auto skeletal = 0; skeletal < skeletal_meshes.count; skeletal++ ) {
            //            auto skeletal_mesh = skeletal_meshes.get( skeletal );
            //            if ( !skeletal_mesh || !utility::is_valid_pointer( skeletal_mesh ) ) {
            //                //logging::print( obf( "chams: skeletal[%d] null, skipping" ), skeletal );
            //                continue;
            //            }

            //            const auto mesh_key = reinterpret_cast< std::uintptr_t >( skeletal_mesh );
            //            if ( s_backups.find( mesh_key ) != s_backups.end( ) )
            //                continue;

            //            const auto mesh_count = skeletal_mesh->get_materials( ).count;
            //            //logging::print( obf( "chams: skeletal[%d] has %d material slot(s)" ), skeletal, mesh_count );

            //            std::vector< std::pair< std::int32_t, engine::u_material_interface* > > slot_backups;
            //            for ( std::int32_t slot = 0; slot < static_cast< std::int32_t >( mesh_count ); slot++ ) {
            //                if ( auto orig = skeletal_mesh->get_material( slot ) )
            //                    slot_backups.push_back( { slot, orig } );
            //            }
            //            if ( !slot_backups.empty( ) )
            //                s_backups[ mesh_key ] = std::move( slot_backups );

            //            for ( auto slot = 0; slot < mesh_count; slot++ ) {
            //                auto mid = skeletal_mesh->create_dynamic_material_instance( slot, material, engine::f_name( ) );
            //                if ( !mid ) {
            //                    //logging::print( obf( "chams: skeletal[%d] slot[%d] create_dynamic_material_instance failed" ), skeletal, slot );
            //                    continue;
            //                }

            //                //mid->set_scalar_parameter_value( g_cache->m_string_library.load( )->conv_wstring_to_name( L"HideElementsOnlyConsidersRedChannel" ), 0.00f );
            //                mid->set_vector_parameter_value( g_cache->m_string_library.load( )->conv_wstring_to_name( L"Glow Color Inner" ), { 10.00f, 0.00f, 0.00f, 1.00f } );
            //                mid->set_vector_parameter_value( g_cache->m_string_library.load( )->conv_wstring_to_name( L"Glow Color Outer" ), { 2.50f, 6.00f, 0.05f, 1.00f } );
            //                mid->set_vector_parameter_value( g_cache->m_string_library.load( )->conv_wstring_to_name( L"FresnelColorReplaceSpec" ), { 0.45f, 0.75f, 0.00f, 1.00f } );
            //                skeletal_mesh->set_material( slot, mid );
            //                //logging::print( obf( "chams: skeletal[%d] slot[%d] applied" ), skeletal, slot );
            //            }
            //        }
            //    }
            //}

			if ( is_on_screen ) {
				const float box_smooth =
					( std::clamp )( config::player::box::corner_smoothness, 0.0f, 1.0f );
				const float box_radius = box_smooth * 15.0f;
				if ( config::player::box::filled )
					esp::box_filled( layout.box, col_pastel, 70, box_radius );
				if ( config::player::box::normal )
					esp::box_outlined( layout.box,
						col_pastel,
						1.5f, 1.f, config::player::box::outline,
						box_radius );
				if ( config::player::box::corner )
					esp::box_corners( layout.box,
						col_pastel,
						0.25f, 1.5f, config::player::box::outline );

				if ( config::player::stats::health_bar ) {
					const auto actor_addr = reinterpret_cast<std::uint64_t>( player_pawn.m_player );
					const auto hst = g_health_thread->get_state( actor_addr );
					if ( hst.init ) {
						bypass::overlay::point_t head_pt{ 0.f, 0.f };
						if ( head_screen )
							head_pt = { static_cast<float>( head_screen->x ),
							            static_cast<float>( head_screen->y ) };
						esp::health::render( hst, g_health_thread->now_s( ), layout, head_pt,
							!player_pawn.m_is_dying );
					}
				}

				if ( config::player::stats::player_name ) {
					const auto& player_name = player_pawn.m_player_name;
					if ( !player_name.empty( ) ) {

						const auto& platform_name = player_pawn.m_platform_name;
						if ( !platform_name.empty( ) ) {
							if ( config::player::stats::platform ) {
								std::wstring unknown_platform;
								const auto get_platform = [ &platform_name, &unknown_platform ]( )
									-> std::pair< const wchar_t*, color_t > {
									if ( strstr( platform_name.c_str( ), obf( "WIN" ) ) )
										return { L"Windows",    { 71,  168, 189, 255 } };
									if ( strstr( platform_name.c_str( ), obf( "XBL" ) ) || strstr( platform_name.c_str( ), obf( "XSX" ) ) )
										return { L"Xbox",       { 156, 176, 128, 255 } };
									if ( strstr( platform_name.c_str( ), obf( "PSN" ) ) || strstr( platform_name.c_str( ), obf( "PS5" ) ) )
										return { L"Playstation", { 0,   144, 193, 255 } };
									if ( strstr( platform_name.c_str( ), obf( "SWT" ) ) )
										return { L"Nintendo",   { 242,  95,  92, 255 } };
									if ( strstr( platform_name.c_str( ), obf( "AND" ) ) )
										return { L"Android",    { 233, 233, 233, 255 } };
									if ( strstr( platform_name.c_str( ), obf( "IOS" ) ) )
										return { L"iPhone",     { 242,  95,  92, 255 } };
									unknown_platform = utility::ansi_to_wstring( platform_name );
									return { unknown_platform.c_str( ), { 138, 129, 124, 255 } };
									};

								auto platform = get_platform( );
								esp::text_small_slot( layout, config::player::stats::slot_platform, platform.first, platform.second, 1.f );
							}


							if ( config::player::stats::rank ) {
								const auto rank_progress = player_pawn.m_rank_progress;
								const auto get_rank = [ rank_progress ]( ) {
									switch ( rank_progress ) {
									case 0:  return std::make_pair( L"Bronze 1", color_t { 232, 146,  33, 255 } );
									case 1:  return std::make_pair( L"Bronze 2", color_t { 232, 146,  33, 255 } );
									case 2:  return std::make_pair( L"Bronze 3", color_t { 232, 146,  33, 255 } );
									case 3:  return std::make_pair( L"Silver 1", color_t { 94,  97, 151, 255 } );
									case 4:  return std::make_pair( L"Silver 2", color_t { 94,  97, 151, 255 } );
									case 5:  return std::make_pair( L"Silver 3", color_t { 94,  97, 151, 255 } );
									case 6:  return std::make_pair( L"Gold 1", color_t { 234,  99,  31, 255 } );
									case 7:  return std::make_pair( L"Gold 2", color_t { 234,  99,  31, 255 } );
									case 8:  return std::make_pair( L"Gold 3", color_t { 234,  99,  31, 255 } );
									case 9:  return std::make_pair( L"Platinum 1", color_t { 65, 200, 189, 255 } );
									case 10: return std::make_pair( L"Platinum 2", color_t { 65, 200, 189, 255 } );
									case 11: return std::make_pair( L"Platinum 3", color_t { 65, 200, 189, 255 } );
									case 12: return std::make_pair( L"Diamond 1", color_t { 46, 196, 220, 255 } );
									case 13: return std::make_pair( L"Diamond 2", color_t { 46, 196, 220, 255 } );
									case 14: return std::make_pair( L"Diamond 3", color_t { 46, 196, 220, 255 } );
									case 15: return std::make_pair( L"Elite", color_t { 88, 112, 157, 255 } );
									case 16: return std::make_pair( L"Champion", color_t { 221,  86,  20, 255 } );
									case 17: return std::make_pair( L"Unreal", color_t { 139,   0, 245, 255 } );
									default: return std::make_pair( L"", color_t { } );
									}
									};

								auto rank = get_rank( );
								if ( std::wcslen( rank.first ) > 1 )
									esp::text_small_slot( layout, config::player::stats::slot_rank, rank.first, rank.second, 1.f );
							}

							if ( config::player::stats::kill_deaths ) {
								if ( stats_cache && stats_cache->has_brief && !stats_cache->brief_text.empty( ) )
									esp::text_small_slot( layout, config::player::stats::slot_kd,
										stats_cache->brief_text.c_str( ), stats_cache->brief_color, 1.f );
							}

							if ( config::player::stats::battle_pass ) {
								if ( stats_cache && stats_cache->has_brief && !stats_cache->battle_pass_text.empty( ) )
									esp::text_small_slot( layout, config::player::stats::slot_battle_pass,
										stats_cache->battle_pass_text.c_str( ), stats_cache->battle_pass_color, 1.f );
							}
						}

						{
							const std::wstring display_name = player_pawn.m_player_name_wide;

							#if 0  /* Old immediate-mode marker + streamsnipe draws — replaced by post-flush block after esp::flush_texts. */
								if ( config::player::stats::streamsnipe && !is_teammate && !is_robot
								&& player_pawn.m_streamer != cache::c_player_pawn::streamer_platform::none ) {

								color_t platform_color {};
								const wchar_t* brand_icon = nullptr;
								switch ( player_pawn.m_streamer ) {
								case cache::c_player_pawn::streamer_platform::tiktok:
									platform_color = { 0, 194, 255, 255 };
									brand_icon = L"\xE07B";
									break;
								case cache::c_player_pawn::streamer_platform::twitch:
									platform_color = { 145, 70, 255, 255 };
									brand_icon = L"\xF1E8";
									break;
								case cache::c_player_pawn::streamer_platform::youtube:
									platform_color = { 255, 0, 0, 255 };
									brand_icon = L"\xF167";
									break;
								default: break;
								}

								const float center_x = layout.box.x + layout.box.width * 0.5f;
								const float eff_top = layout.top_offset == 0.f ? esp::helper::top_gap : layout.top_offset;

								if ( brand_icon ) {
									//bypass::overlay::filled_rectangle(
									//	{ center_x - 14.f, layout.box.y - eff_top - esp::text_medium_size - 3.f, 28.f, 3.f },
									//	platform_color );

									constexpr float icon_gap = 8.f;
									const float name_half_w = min(
										static_cast< float >( player_pawn.m_player_name_wide.length( ) ) * esp::text_medium_size * 0.62f * 0.5f,
										esp::helper::area_width * 0.5f );

									bypass::overlay::text_ex(
										brand_icon,
										{ center_x + name_half_w + icon_gap,
										  layout.box.y - eff_top - esp::text_medium_size,
										  esp::text_medium_size, esp::text_medium_size + 4.f },
										platform_color,
										esp::text_medium_size + 1.f,
										text_weight_t::regular,
										L"Font Awesome Brands",
										text_align_t::left,
										false );
								}
							}

							if ( config::player::stats::team_marker && !is_teammate && !is_robot ) {
								const color_t tc = team_color( player_pawn.m_team_index );

								const float name_half_w = min(
									static_cast< float >( player_pawn.m_player_name_wide.length( ) ) * esp::text_medium_size * 0.6f * 0.5f,
									esp::helper::area_width * 0.5f );
								const float center_x = layout.box.x + layout.box.width * 0.5f;

								const float eff_top = layout.top_offset == 0.f ? esp::helper::top_gap : layout.top_offset;
								const bypass::overlay::point_t marker_pt {
									center_x - name_half_w - 11.f,
									layout.box.y - eff_top - esp::text_medium_size * 0.5f
								};
								bypass::overlay::circle( marker_pt, 4.5f, { 0, 0, 0, 200 }, 1.5f );
								bypass::overlay::filled_circle( marker_pt, 3.f, tc );
							}

							#endif  /* end of old immediate marker + streamsnipe */

								// Pre-compute the background-plate paddings so the
								// name plate (drawn inside flush_texts) wraps the
								// team-color circle on its left and the streamsnipe
								// brand icon on its right - with a gap between each
								// adornment and the plate edge. Same conditions the
								// adornments block below uses; kept in sync manually.
								float name_bg_left_pad  = 0.f;
								float name_bg_right_pad = 0.f;
								{
									// Team circle: outline radius 4.5, sits ~11 px
									// left of the name text edge. So it needs ~17 px
									// of pad past the plate's default -3 to sit
									// entirely inside, plus a 4 px gap.
									if ( config::player::stats::team_marker && !is_teammate && !is_robot )
										name_bg_left_pad = 17.f;

									// Streamsnipe brand icon: rendered with a font
									// size of text_medium_size+1 and an 8 px gap
									// from the text edge. Icon glyph advance ~= 14 px.
									// 8 (gap-to-icon) + 14 (icon) + 4 (plate-to-icon
									// inner gap) - 3 (default plate right padding)
									// ≈ 23 px extension.
									if ( config::player::stats::streamsnipe && !is_teammate && !is_robot
										&& player_pawn.m_streamer != cache::c_player_pawn::streamer_platform::none )
										name_bg_right_pad = 23.f;
								}

								esp::text_medium_slot( layout, config::player::stats::slot_name,
									player_pawn.m_player_name_wide.c_str( ), col_primary, 1.f,
									name_bg_left_pad, name_bg_right_pad );
						}
					}
				}

				if ( config::player::skeleton::normal ) {
					if ( config::player::skeleton::curve_smoothness <= 0.0001f )
						esp::skeleton_cpu(
							player_pawn.m_skeleton_positions,
							col_primary,
							1.5f,
							config::player::skeleton::outline );
					else
						esp::skeleton_curved_cpu(
							player_pawn.m_skeleton_positions,
							col_primary,
							1.5f,
							config::player::skeleton::outline );
				}
				else if ( config::player::skeleton::curved ) {
					if ( config::player::skeleton::curve_smoothness <= 0.0001f )
						esp::skeleton(
							player_pawn.m_skeleton_positions,
							col_primary,
							1.5f,
							config::player::skeleton::outline );
					else
						esp::skeleton_curved(
							player_pawn.m_skeleton_positions,
							col_primary,
							1.5f,
							config::player::skeleton::outline );
				}

				if ( config::player::skeleton::fingers )
					esp::fingers(
						player_pawn.m_finger_positions,
						col_primary,
						0.8f,
						config::player::skeleton::outline );

				if ( config::aimbot::visuals::aim_indicator && head_screen ) {
					const auto fwd     = player_pawn.m_root_rotation.get_forward_vector( );
					const auto to_us   = cache::camera_location - player_pawn.m_root_location;
					const auto to_len  = to_us.length( );
					if ( to_len > 1e-6 ) {
						const auto  to_norm    = to_us / to_len;
						const double dot_us    = fwd.vector_scalar( to_norm );
						const double angle_deg = static_cast< double >( config::aimbot::visuals::aim_indicator_angle );
						const double cos_ok    = std::cos( angle_deg * std::numbers::pi / 180.0 );
						if ( dot_us >= cos_ok ) {
							constexpr bypass::overlay::color_t aim_hot   { 255,  60,  60, 255 };
							constexpr bypass::overlay::color_t aim_glow  { 255,  90,  90,  60 };
							constexpr bypass::overlay::color_t aim_stroke{   0,   0,   0, 200 };
							const float hx = static_cast< float >( head_screen->x );
							const float hy = static_cast< float >( head_screen->y ) - 22.0f;
							constexpr float r_outer = 7.5f;
							constexpr float r_inner = 3.5f;
							bypass::overlay::circle( { hx, hy }, r_outer + 3.0f, aim_glow,   3.0f );
							bypass::overlay::circle( { hx, hy }, r_outer + 1.0f, aim_stroke, 1.5f );
							bypass::overlay::circle( { hx, hy }, r_outer,        aim_hot,    1.5f );
							bypass::overlay::filled_circle( { hx, hy }, r_inner, aim_hot );
							constexpr float tick_gap = 2.0f;
							constexpr float tick_len = 4.5f;
							bypass::overlay::line( { hx, hy - r_outer - tick_gap             }, { hx, hy - r_outer - tick_gap - tick_len }, aim_hot, 1.5f );
							bypass::overlay::line( { hx, hy + r_outer + tick_gap             }, { hx, hy + r_outer + tick_gap + tick_len }, aim_hot, 1.5f );
							bypass::overlay::line( { hx - r_outer - tick_gap,             hy }, { hx - r_outer - tick_gap - tick_len,   hy }, aim_hot, 1.5f );
							bypass::overlay::line( { hx + r_outer + tick_gap,             hy }, { hx + r_outer + tick_gap + tick_len,   hy }, aim_hot, 1.5f );
						}
					}
				}

				const auto& weapon_name = player_pawn.m_weapon_name;
				if ( !weapon_name.empty( ) ) {
					if ( config::player::stats::inventory ) {
						const auto player_key = reinterpret_cast< std::uint64_t >( player_pawn.m_mesh );
						esp::inventory::track( player_key, weapon_name, player_pawn.m_weapon );
						esp::inventory::render( player_key, layout, 1.f );

						const auto inv_ammo = player_pawn.m_ammo_count;
						if ( config::player::stats::weapon_ammo && inv_ammo ) {
							const auto inv_magazine = ( stats_cache && stats_cache->weapon_name == weapon_name )
								? stats_cache->weapon_magazine
								: 0u;
							const auto ammo_text = inv_magazine
								? std::format( L"{}/{} ammo", inv_ammo, inv_magazine )
								: std::format( L"{} ammo", inv_ammo );
							esp::text_small_slot( layout, config::player::stats::slot_weapon_ammo,
								ammo_text.c_str( ), { 60, 140, 110, 255 }, 1.f );
						}
					}
					else if ( config::player::stats::weapon_name ) {
						//auto rarity = player_pawn.m_rarity;
						//const auto get_rarity_col = [ rarity ]( ) {
						//	switch ( rarity ) {
						//	case engine::e_fort_rarity::common:       return bypass::overlay::color_t( 178, 174, 190, 255 );
						//	case engine::e_fort_rarity::uncommon:     return bypass::overlay::color_t( 88, 178, 108, 255 );
						//	case engine::e_fort_rarity::rare:         return bypass::overlay::color_t( 100, 138, 255, 255 );
						//	case engine::e_fort_rarity::epic:         return bypass::overlay::color_t( 195, 115, 255, 255 );
						//	case engine::e_fort_rarity::legendary:    return bypass::overlay::color_t( 255, 183, 68, 255 );
						//	case engine::e_fort_rarity::mythic:       return bypass::overlay::color_t( 255, 212, 42, 255 );
						//	case engine::e_fort_rarity::transcendent: return bypass::overlay::color_t( 112, 208, 255, 255 );
						//	default:                                  return bypass::overlay::color_t( 178, 174, 190, 255 );
						//	}
						//	};

						const auto ammo_count    = player_pawn.m_ammo_count;
						const auto weapon_magazine = ( stats_cache && stats_cache->weapon_name == weapon_name )
							? stats_cache->weapon_magazine
							: 0u;
						const auto& wide_weapon_name = player_pawn.m_weapon_name_wide;
						// show_ammo gates the "(current[/max])" suffix on the
						// name-only display path; toggling Weapon ammo off
						// leaves the name bare regardless of what the pawn
						// actually has loaded.
						const bool show_ammo = config::player::stats::weapon_ammo && ammo_count > 0;
						const auto weapon_text = !show_ammo
							? std::format( L"{}", wide_weapon_name )
							: weapon_magazine
							? std::format( L"{} ({}/{})", wide_weapon_name, ammo_count, weapon_magazine )
							: std::format( L"{} ({})", wide_weapon_name, ammo_count );

						esp::text_medium_slot( layout, config::player::stats::slot_weapon_name, weapon_text.c_str( ), { 190, 190, 190, 255 }, 1.f );
					}

					if ( config::player::stats::weapon_damage ) {
						if ( stats_cache && stats_cache->weapon_name == weapon_name && stats_cache->has_weapon_damage ) {
							esp::text_small_slot( layout, config::player::stats::slot_weapon_damage,
								stats_cache->weapon_damage_text.c_str( ), { 255, 70, 70, 255 }, 1.f );
						}
					}
				}

				if ( config::player::stats::state ) {
					const auto get_state = [ player_pawn ]( ) {
						const auto building_state = player_pawn.m_building_state;
						switch ( building_state ) {
						case 0:
							return std::make_pair( static_cast<const wchar_t*>( obf( L"Building" ) ), color_t { 30, 144, 255, 255 } ); break;
						case 1:
							return std::make_pair( static_cast<const wchar_t*>( obf( L"Editing" ) ), color_t { 250, 131, 52, 255 } ); break;
						default:
							if ( player_pawn.m_is_skydiving )
								return std::make_pair( static_cast< const wchar_t* >( obf( L"Skydiving" ) ), color_t { 80, 200, 240, 255 } );
							return std::make_pair( static_cast<const wchar_t*>( obf( L"" ) ), color_t { } );
							break;
						}
						};

					const auto state = get_state( );
					if ( std::wcslen( state.first ) > 1 )
						esp::text_medium_slot( layout, config::player::stats::slot_state, state.first, state.second, 1.f );
				}

				// Reloading tag — accent-coloured label gated by the
				// weapon_reloading config toggle AND the cached
				// m_is_reloading flag (actors.h reads bit 0 of the
				// byte at a_fort_weapon + 0x0359 each medium tick).
				if ( config::player::stats::weapon_reloading && player_pawn.m_is_reloading ) {
					esp::text_medium_slot( layout, config::player::stats::slot_reloading,
						static_cast< const wchar_t* >( obf( L"Reloading" ) ),
						col_accent, 1.f );
				}

				if ( config::player::stats::distance ) {
					const auto distance_text = std::format( L"{:.1f}m", distance );
					esp::text_medium_slot( layout, config::player::stats::slot_distance, distance_text.c_str( ), col_primary, 1.f );
				}

				// Head-circle block removed 2026-08-26 (per user
				// request). The Smoothness slider under the Skeleton
				// child took its slot.
			}

			if ( any_corner && config::player::snaplines::enabled ) {
				const float snap_cx = layout.box.x + layout.box.width * 0.5f;
				const float snap_ty = config::player::snaplines::position == 0 ? layout.box.y
					: config::player::snaplines::position == 1 ? layout.box.y + layout.box.height * 0.5f
					: layout.box.y + layout.box.height;
				const bypass::overlay::point_t snap_from { static_cast<float>( gui::screen_width_center ), static_cast<float>( gui::screen_height ) };
				const bypass::overlay::point_t snap_to   { snap_cx, snap_ty };

				const float snap_curve = ( std::clamp )( config::player::snaplines::smoothing, 0.0f, 1.0f );
				if ( snap_curve <= 0.0001f ) {
					if ( config::player::snaplines::outline )
						bypass::overlay::line_async( snap_from, snap_to, { 0, 0, 0, 180 }, 2.5f );
					bypass::overlay::line_async( snap_from, snap_to, col_primary, 1.0f );
				}
				else {
					const float dx = snap_to.x - snap_from.x;
					const float dy = snap_to.y - snap_from.y;
					const float len = std::sqrt( dx * dx + dy * dy );
					const float nx = len > 1e-4f ? -dy / len : 0.0f;
					const float ny = len > 1e-4f ?  dx / len : 0.0f;
					const float bulge = len * snap_curve * 0.5f;
					const float mid_x = ( snap_from.x + snap_to.x ) * 0.5f + nx * bulge;
					const float mid_y = ( snap_from.y + snap_to.y ) * 0.5f + ny * bulge;

					constexpr int kSeg = 16;
					bypass::overlay::point_t prev = snap_from;
					const bypass::overlay::color_t outline_col { 0, 0, 0, 180 };
					for ( int i = 1; i <= kSeg; ++i ) {
						const float t = static_cast< float >( i ) / static_cast< float >( kSeg );
						const float u = 1.0f - t;
						const float x = u * u * snap_from.x + 2.0f * u * t * mid_x + t * t * snap_to.x;
						const float y = u * u * snap_from.y + 2.0f * u * t * mid_y + t * t * snap_to.y;
						const bypass::overlay::point_t cur { x, y };
						if ( config::player::snaplines::outline )
							bypass::overlay::line_async( prev, cur, outline_col, 2.5f );
						bypass::overlay::line_async( prev, cur, col_primary, 1.0f );
						prev = cur;
					}
				}
			}

			if ( config::aimbot::enabled && g_cache->m_acknowledged_pawn.load( ) ) {
				if ( config::aimbot::visuals::fov_arrows ) {
					if ( head_screen ) {
						const float fov_r = g_aimbot->get_fov( );
						if ( fov_r > 0.0f ) {
							const float cx = static_cast< float >( gui::screen_width_center );
							const float cy = static_cast< float >( gui::screen_height_center );
							const float hx = static_cast< float >( head_screen->x );
							const float hy = static_cast< float >( head_screen->y );
							const float adx = hx - cx, ady = hy - cy;
							if ( std::hypot( adx, ady ) > fov_r ) {
								const float angle = std::atan2( ady, adx );
								const bypass::overlay::point_t arrow_pos {
									cx + ( fov_r + 12.0f ) * std::cos( angle ),
									cy + ( fov_r + 12.0f ) * std::sin( angle )
								};
								const bypass::overlay::point_t v0 {
									arrow_pos.x + std::cos( angle ) * 15.0f,
									arrow_pos.y + std::sin( angle ) * 15.0f
								};
								const bypass::overlay::point_t v1 {
									arrow_pos.x + std::cos( angle - 1.5f ) * 12.0f,
									arrow_pos.y + std::sin( angle - 1.5f ) * 12.0f
								};
								const bypass::overlay::point_t v2 {
									arrow_pos.x + std::cos( angle + 1.5f ) * 12.0f,
									arrow_pos.y + std::sin( angle + 1.5f ) * 12.0f
								};
								const auto arrow_col = col_primary;
								for ( float s = 0.0f; s <= 1.0f; s += 0.1f ) {
									bypass::overlay::line_async(
										{ v0.x + ( v1.x - v0.x ) * s, v0.y + ( v1.y - v0.y ) * s },
										{ v0.x + ( v2.x - v0.x ) * s, v0.y + ( v2.y - v0.y ) * s },
										arrow_col, 1.5f );
								}
								constexpr bypass::overlay::color_t arrow_outline { 0, 0, 0, 200 };
								bypass::overlay::line_async( v0, v1, arrow_outline, 2.5f );
								bypass::overlay::line_async( v1, v2, arrow_outline, 2.5f );
								bypass::overlay::line_async( v2, v0, arrow_outline, 2.5f );
							}
						}
					}
				}
			}

			if ( config::aimbot::visuals::threat_warning && g_cache->m_acknowledged_pawn.load( ) ) {
				const double threat_dist = static_cast< double >( config::aimbot::visuals::threat_distance );
				if ( distance <= threat_dist ) {
					const bool off_screen = !head_screen ||
						head_screen->x < 0.0 || head_screen->x > static_cast< double >( gui::screen_width ) ||
						head_screen->y < 0.0 || head_screen->y > static_cast< double >( gui::screen_height );

					if ( off_screen ) {
						auto root_rotation = player_pawn.m_root_rotation;
						const auto fwd      = root_rotation.get_forward_vector( );
						const auto to_local = cache::camera_location - player_pawn.m_root_location;
						const auto to_len   = to_local.length( );
						if ( to_len > 1e-6 ) {
							const auto to_norm = to_local / to_len;
							const double dot = fwd.vector_scalar( to_norm );
							const double cos_thresh = std::cos(
								static_cast< double >( config::aimbot::visuals::threat_fov ) *
								std::numbers::pi / 180.0 );

							if ( dot >= cos_thresh ) {
								// (Sound-alert block removed 2026-08-26.
								// User replaced it with the aim-indicator
								// feature — see below in this same
								// render() where per-player aim
								// direction is checked and a marker is
								// drawn over anyone lining you up.)

								const auto delta = player_pawn.m_root_location - cache::camera_location;
								const float tx = static_cast< float >( delta.vector_scalar( cache::camera_axis_right ) );
								const float ty = static_cast< float >( -delta.vector_scalar( cache::camera_axis_up ) );
								const float tz = static_cast< float >( delta.vector_scalar( cache::camera_axis_forward ) );
								const float angle = ( tz >= 0.0f ) ? std::atan2( ty, tx ) : std::atan2( -ty, -tx );
								const float dx = std::cos( angle ), dy = std::sin( angle );

								const float cx  = static_cast< float >( gui::screen_width_center );
								const float cy  = static_cast< float >( gui::screen_height_center );
								const float sw  = static_cast< float >( gui::screen_width );
								const float sh  = static_cast< float >( gui::screen_height );
								constexpr float pad = 50.0f;

								float t_edge = FLT_MAX;
								if ( dx >  1e-6f ) t_edge = min( t_edge, ( sw - pad - cx ) / dx );
								if ( dx < -1e-6f ) t_edge = min( t_edge, ( pad - cx ) / dx );
								if ( dy >  1e-6f ) t_edge = min( t_edge, ( sh - pad - cy ) / dy );
								if ( dy < -1e-6f ) t_edge = min( t_edge, ( pad - cy ) / dy );

								if ( t_edge > 0.0f && t_edge < FLT_MAX ) {
									const float ex = cx + dx * t_edge;
									const float ey = cy + dy * t_edge;
									const auto pkey = reinterpret_cast< std::uint64_t >( player_pawn.m_player );
									esp::draw_threat_arrow(
										pkey, ex, ey, cx, cy,
										static_cast< float >( distance ),
										esp::dpi_scale( ) );
								}
							}
						}
					}
				}
			}

			// Flush deferred text stack — draws every enabled slot
			// that had data this frame, sorted by config order,
			// stacking cleanly with NO gaps for empty/disabled slots.
			// Also populates esp::g_slot_y_offset[slot] so the side
			// adornments below (team-color circle, streamsnipe brand
			// icon) can align to the actual y of the name text no
			// matter where it lands in the reordered stack.
			esp::flush_texts( layout );

			// ── Adornments aligned to the drawn name text ────────
			// Both draw at the vertical center of the name text row,
			// using g_slot_y_offset[slot_name] which flush_texts just
			// populated. Only fire if the name was actually drawn
			// (g_pending[slot_name].valid) AND the ESP option is on.
			if ( esp::g_pending[ config::player::stats::slot_name ].valid ) {
				const float name_top_y    = layout.box.y - esp::g_slot_y_offset[ config::player::stats::slot_name ];
				const float name_center_y = name_top_y + esp::text_medium_size * 0.5f;
				const float center_x      = layout.box.x + layout.box.width * 0.5f;

				// Team-color circle (left of the name text).
				if ( config::player::stats::team_marker && !is_teammate && !is_robot ) {
					const color_t tc = team_color( player_pawn.m_team_index );
					const float name_half_w = min(
						static_cast< float >( player_pawn.m_player_name_wide.length( ) ) * esp::text_medium_size * 0.6f * 0.5f,
						esp::helper::area_width * 0.5f );
					const bypass::overlay::point_t marker_pt {
						center_x - name_half_w - 11.f,
						name_center_y
					};
					bypass::overlay::circle( marker_pt, 4.5f, { 0, 0, 0, 200 }, 1.5f );
					bypass::overlay::filled_circle( marker_pt, 3.f, tc );
				}

				// Streamsnipe brand icon (right of the name text).
				if ( config::player::stats::streamsnipe && !is_teammate && !is_robot
					&& player_pawn.m_streamer != cache::c_player_pawn::streamer_platform::none ) {
					color_t platform_color {};
					const wchar_t* brand_icon = nullptr;
					switch ( player_pawn.m_streamer ) {
					case cache::c_player_pawn::streamer_platform::tiktok:
						platform_color = { 0, 194, 255, 255 };
						brand_icon = L"\xE07B";
						break;
					case cache::c_player_pawn::streamer_platform::twitch:
						platform_color = { 145, 70, 255, 255 };
						brand_icon = L"\xF1E8";
						break;
					case cache::c_player_pawn::streamer_platform::youtube:
						platform_color = { 255, 0, 0, 255 };
						brand_icon = L"\xF167";
						break;
					default: break;
					}
					if ( brand_icon ) {
						constexpr float icon_gap = 8.f;
						const float name_half_w = min(
							static_cast< float >( player_pawn.m_player_name_wide.length( ) ) * esp::text_medium_size * 0.62f * 0.5f,
							esp::helper::area_width * 0.5f );
						bypass::overlay::text_ex(
							brand_icon,
							{ center_x + name_half_w + icon_gap,
							  name_top_y,
							  esp::text_medium_size, esp::text_medium_size + 4.f },
							platform_color,
							esp::text_medium_size + 1.f,
							text_weight_t::regular,
							L"Font Awesome Brands",
							text_align_t::left,
							false );
					}
				}
			}

        }
    }
}
