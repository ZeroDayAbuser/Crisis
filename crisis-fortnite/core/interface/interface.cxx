#include <deps/includes.h>

#pragma comment( lib, "winmm.lib" )
extern "C" __declspec( dllimport ) UINT  WINAPI timeBeginPeriod( UINT );
extern "C" __declspec( dllimport ) UINT  WINAPI timeEndPeriod  ( UINT );
extern "C" __declspec( dllimport ) DWORD WINAPI mciSendStringW ( const wchar_t*, wchar_t*, unsigned int, void* );

namespace gui::stages {
	enum class stage_type_t { interactive, display };

	struct stage_t {
		const char*                     name;
		stage_type_t                    type;
		bool                            show_background;
		std::function< void( float ) >  draw;
	};

	constexpr float bg_alpha_interactive = 0.35f;
	constexpr float bg_alpha_display     = 0.55f;
	constexpr float bg_alpha_hidden      = 0.00f;

	constexpr float bg_lerp_k       = 2.6f;
	constexpr float content_lerp_k  = 3.5f;

	inline std::vector< stage_t > g_stages;
	inline int   g_current   = -1;
	inline int   g_next      = -1;
	inline float g_progress  = 1.f;
	inline bool  g_fade_body = true;
	inline float g_bg_alpha  = 0.f;
	inline float g_bg_target = 0.f;

	inline std::chrono::steady_clock::time_point g_last_tick;
	inline bool  g_ticks_initialized = false;

	inline int register_stage( stage_t s ) {
		g_stages.push_back( std::move( s ) );
		return static_cast< int >( g_stages.size( ) ) - 1;
	}

	inline void update_bg_target( int idx ) {
		if ( idx < 0 || idx >= static_cast< int >( g_stages.size( ) ) ) {
			g_bg_target = bg_alpha_hidden;
			return;
		}
		const auto& s = g_stages[ idx ];
		if ( !s.show_background ) {
			g_bg_target = bg_alpha_hidden;
			return;
		}
		g_bg_target = ( s.type == stage_type_t::interactive )
			? bg_alpha_interactive : bg_alpha_display;
	}

	inline void switch_to( int idx, bool fade_content = true ) {
		if ( idx == g_next ) return;
		if ( idx == g_current && g_next == -1 ) {
			update_bg_target( idx );
			return;
		}
		if ( g_next >= 0 )
			g_current = g_next;

		g_next      = idx;
		g_fade_body = fade_content;
		g_progress  = fade_content ? 0.f : 1.f;
		update_bg_target( idx );
	}

	inline void tick_and_draw_background( ) {
		const auto now = std::chrono::steady_clock::now( );
		if ( !g_ticks_initialized ) {
			g_last_tick         = now;
			g_ticks_initialized = true;
		}
		float dt = std::chrono::duration< float >( now - g_last_tick ).count( );
		g_last_tick = now;
		if ( dt < 0.f )    dt = 0.f;
		if ( dt > 0.25f )  dt = 0.25f;

		const float bg_factor = 1.f - std::exp( -bg_lerp_k * dt );
		g_bg_alpha += ( g_bg_target - g_bg_alpha ) * bg_factor;

		if ( g_next >= 0 ) {
			if ( g_fade_body ) {
				g_progress += content_lerp_k * dt;
				if ( g_progress >= 1.f ) {
					g_progress = 1.f;
					g_current  = g_next;
					g_next     = -1;
				}
			}
			else {
				g_current  = g_next;
				g_next     = -1;
				g_progress = 1.f;
			}
		}

		if ( g_bg_alpha > 0.002f ) {
			std::int32_t sw = 0, sh = 0;
			bypass::overlay::get_size( sw, sh );
			const auto a =
				static_cast< std::uint8_t >( g_bg_alpha * 255.f );
			bypass::overlay::filled_rectangle(
				{ 0.f, 0.f, static_cast< float >( sw ), static_cast< float >( sh ) },
				{ 8, 8, 12, a } );
		}
	}

	inline void draw_content( ) {
		if ( g_next >= 0 && g_fade_body ) {
			const float old_a = 1.f - g_progress;
			const float new_a = g_progress;
			if ( g_current >= 0 && old_a > 0.002f )
				g_stages[ g_current ].draw( old_a );
			if ( new_a > 0.002f )
				g_stages[ g_next ].draw( new_a );
			return;
		}
		if ( g_current >= 0 )
			g_stages[ g_current ].draw( 1.f );
	}
}

namespace gui::demo {
	inline int  welcome_idx     = -1;
	inline int  interactive_idx = -1;
	inline bool inited          = false;

	inline std::chrono::steady_clock::time_point welcome_start;
	inline std::chrono::steady_clock::time_point content_start;
	inline bool                                  content_running = false;

	inline bool s_menu_enabled = true;
	inline bool s_menu_prev    = true;

	constexpr std::uint32_t logo_image_id = 0x51AC'C0DEu;
	inline bool logo_registered = false;

	constexpr float welcome_total_seconds = 12.0f;
	constexpr float k_wav_trigger_t       =  5.8f;

	inline bool  g_wav_triggered = false;
	inline bool  g_wav_opened    = false;
	inline int   g_wav_len_ms    = 0;
	inline std::chrono::steady_clock::time_point g_wav_play_start;

	inline bool  g_notif_done     = true;
	inline bool  g_notif_released = false;
	inline std::chrono::steady_clock::time_point g_notif_release_time;

	inline void reset( ) {
		if ( g_wav_opened ) {
			mciSendStringW( L"stop crisisbgm",  nullptr, 0, nullptr );
			mciSendStringW( L"close crisisbgm", nullptr, 0, nullptr );
			g_wav_opened = false;
		}
		g_wav_triggered = false;
		g_wav_len_ms    = 0;
		menu::notifications::finish_pinned( );
		g_notif_done     = true;
		g_notif_released = false;

		content_running             = false;
		welcome_start               = std::chrono::steady_clock::now( );
		stages::g_current           = welcome_idx;
		stages::g_next              = -1;
		stages::g_progress          = 1.f;
		stages::g_fade_body         = true;
		stages::g_bg_alpha          = 0.f;
		stages::g_ticks_initialized = false;
		stages::update_bg_target( welcome_idx );
	}

	inline bool interactive_stage_active( ) {
		return stages::g_current == interactive_idx
		    || stages::g_next    == interactive_idx;
	}

	inline void apply_menu_bg_target( ) {
		if ( !interactive_stage_active( ) ) return;
		stages::g_bg_target = s_menu_enabled
			? stages::bg_alpha_interactive
			: stages::bg_alpha_hidden;
	}

	struct v2_t { float x, y; };

	inline float clamp01( float v ) {
		return v < 0.f ? 0.f : ( v > 1.f ? 1.f : v );
	}
	inline float phase01( float now, float start, float end ) {
		if ( end <= start ) return now >= start ? 1.f : 0.f;
		return clamp01( ( now - start ) / ( end - start ) );
	}
	inline float ease_out_cubic( float t ) {
		const float u = 1.f - clamp01( t );
		return 1.f - u * u * u;
	}
	inline float ease_out_back( float t ) {
		t = clamp01( t );
		constexpr float c1 = 1.70158f;
		constexpr float c3 = c1 + 1.f;
		const float u = t - 1.f;
		return 1.f + c3 * u * u * u + c1 * u * u;
	}
	inline float lerpf( float a, float b, float t ) { return a + ( b - a ) * t; }
	inline v2_t rot2( v2_t p, float rad ) {
		const float c = std::cos( rad );
		const float s = std::sin( rad );
		return { p.x * c - p.y * s, p.x * s + p.y * c };
	}

	inline void draw_bezier_strand(
		v2_t p0, v2_t p1, v2_t p2, v2_t p3,
		bypass::overlay::color_t head_col,
		bypass::overlay::color_t tail_col,
		float head_alpha,
		float tail_u, float head_u,
		float head_thick, float tail_thick,
		int segs,
		float tail_alpha_frac = 0.12f )
	{
		if ( head_u <= tail_u || head_alpha <= 0.001f ) return;

		auto bezier = [ & ]( float u ) -> v2_t {
			const float v  = 1.f - u;
			const float v2 = v * v;
			const float v3 = v2 * v;
			const float u2 = u * u;
			const float u3 = u2 * u;
			return {
				v3 * p0.x + 3.f * v2 * u * p1.x + 3.f * v * u2 * p2.x + u3 * p3.x,
				v3 * p0.y + 3.f * v2 * u * p1.y + 3.f * v * u2 * p2.y + u3 * p3.y
			};
		};

		auto lerp_col = [ ]( bypass::overlay::color_t a, bypass::overlay::color_t b, float t ) -> bypass::overlay::color_t {
			return {
				static_cast< std::uint8_t >( static_cast< float >( a.r ) + ( static_cast< float >( b.r ) - static_cast< float >( a.r ) ) * t ),
				static_cast< std::uint8_t >( static_cast< float >( a.g ) + ( static_cast< float >( b.g ) - static_cast< float >( a.g ) ) * t ),
				static_cast< std::uint8_t >( static_cast< float >( a.b ) + ( static_cast< float >( b.b ) - static_cast< float >( a.b ) ) * t ),
				255
			};
		};

		const float arc_span = head_u - tail_u;
		for ( int i = 0; i < segs; ++i ) {
			const float fa = static_cast< float >( i     ) / static_cast< float >( segs );
			const float fb = static_cast< float >( i + 1 ) / static_cast< float >( segs );
			const float ua = clamp01( tail_u + fa * arc_span );
			const float ub = clamp01( tail_u + fb * arc_span );
			if ( ua >= 1.f ) break;

			const auto pa = bezier( ua );
			const auto pb = bezier( ub );

			const float seg_t     = fa;
			const float seg_alpha = head_alpha * ( tail_alpha_frac + ( 1.f - tail_alpha_frac ) * seg_t );
			auto        col       = lerp_col( tail_col, head_col, seg_t );
			col.a = static_cast< std::uint8_t >( clamp01( seg_alpha ) * 255.f );

			const float seg_thick = tail_thick + ( head_thick - tail_thick ) * seg_t;

			bypass::overlay::line_async( { pa.x, pa.y }, { pb.x, pb.y }, col, seg_thick );
		}
	}

	inline void draw_welcome( float alpha ) {
		std::int32_t sw = 0, sh = 0;
		bypass::overlay::get_size( sw, sh );
		const float w = static_cast< float >( sw );
		const float h = static_cast< float >( sh );
		const float scale = ( std::min )( w / 1920.f, h / 1080.f );
		const v2_t center { w * 0.5f, h * 0.5f };

		if ( !content_running ) {
			if ( stages::g_bg_target > 0.01f &&
			     stages::g_bg_alpha >= stages::g_bg_target * 0.9f ) {
				content_running = true;
				content_start   = std::chrono::steady_clock::now( );
			}
			else {
				return;
			}
		}

		const float t = std::chrono::duration< float >(
			std::chrono::steady_clock::now( ) - content_start ).count( );

		constexpr bypass::overlay::color_t accent_blue   { 76,  139, 255, 255 };
		constexpr bypass::overlay::color_t accent_soft   { 123, 163, 232, 255 };
		constexpr bypass::overlay::color_t accent_violet { 168, 85,  247, 255 };
		constexpr bypass::overlay::color_t text_light    { 232, 236, 255, 255 };
		constexpr bypass::overlay::color_t text_muted    { 168, 178, 210, 255 };

		{
			const float bracket_inset  = 48.f  * scale;
			const float bracket_arm    = 64.f  * scale;
			const float bracket_thick  = 2.f;
			const float bracket_slide  = 20.f  * scale;
			constexpr float bracket_stagger = 0.10f;

			for ( int i = 0; i < 4; ++i ) {
				const float local_start = i * bracket_stagger;
				const float local_end   = 0.55f + local_start;
				const float raw_p       = phase01( t, local_start, local_end );
				const float p_scale     = ease_out_back( raw_p );
				const float p_slide     = ease_out_cubic( raw_p );
				const float slide       = lerpf( bracket_slide, 0.f, p_slide );
				const float s           = lerpf( 0.6f, 1.f, p_scale );

				float steady = 1.f;
				if ( t > local_end ) {
					const float breathe =
						0.5f + 0.5f * std::sin( ( t - local_end ) * 6.28318530718f / 3.f );
					steady = 0.85f + 0.15f * breathe;
				}
				else {
					steady = raw_p;
				}

				v2_t corner {}, dh {}, dv {};
				switch ( i ) {
					case 0:
						corner = { bracket_inset + slide, bracket_inset + slide };
						dh = { 1.f, 0.f }; dv = { 0.f, 1.f }; break;
					case 1:
						corner = { w - bracket_inset - slide, bracket_inset + slide };
						dh = { -1.f, 0.f }; dv = { 0.f, 1.f }; break;
					case 2:
						corner = { w - bracket_inset - slide, h - bracket_inset - slide };
						dh = { -1.f, 0.f }; dv = { 0.f, -1.f }; break;
					default:
						corner = { bracket_inset + slide, h - bracket_inset - slide };
						dh = { 1.f, 0.f }; dv = { 0.f, -1.f }; break;
				}

				const float ba = alpha * steady;
				const auto  bcol = bypass::overlay::color_t {
					accent_blue.r, accent_blue.g, accent_blue.b,
					static_cast< std::uint8_t >( clamp01( ba ) * 255.f ) };

				const float arm = bracket_arm * s;
				bypass::overlay::line_async(
					{ corner.x, corner.y },
					{ corner.x + dh.x * arm, corner.y + dh.y * arm },
					bcol, bracket_thick );
				bypass::overlay::line_async(
					{ corner.x, corner.y },
					{ corner.x + dv.x * arm, corner.y + dv.y * arm },
					bcol, bracket_thick );

				const float tick_off  = 6.f * scale;
				const float tick_len  = 4.f * scale;
				const v2_t  tf { corner.x + dh.x * tick_off, corner.y + dh.y * tick_off };
				const v2_t  tt { tf.x + dv.x * tick_len, tf.y + dv.y * tick_len };
				bypass::overlay::line_async(
					{ tf.x, tf.y }, { tt.x, tt.y }, bcol, 1.f );
			}
		}

		{
			constexpr int   wind_n       = 16;
			constexpr float wind_x_pad   = 130.f;
			constexpr float wind_ctrl_k  = 0.81f;

			constexpr bypass::overlay::color_t wind_in  { 168, 85,  247, 255 };
			constexpr bypass::overlay::color_t wind_out { 76,  139, 255, 255 };

			auto hf = []( int sd, int salt ) -> float {
				unsigned s = static_cast< unsigned >( sd ) * 1664525u + static_cast< unsigned >( salt ) * 1013904223u + 22695477u;
				s ^= s >> 16;
				s *= 0x45d9f3bu;
				s ^= s >> 16;
				return static_cast< float >( s & 0xFFFFu ) / 65535.f;
			};

			for ( int i = 0; i < wind_n; ++i ) {
				const float h0 = hf( i, 0 );
				const float h1 = hf( i, 1 );
				const float h2 = hf( i, 2 );
				const float h3 = hf( i, 3 );

				const float y_frac   = ( static_cast< float >( i ) + 0.5f ) / static_cast< float >( wind_n );
				const float y_off    = ( y_frac - 0.5f ) * h * 0.88f;
				const float y_in     = center.y + y_off;
				const float y_out    = center.y + y_off * ( 0.70f + h1 * 0.60f );

				const float cy1 = lerpf( y_in,  center.y, wind_ctrl_k * ( 0.68f + h0 * 0.32f ) );
				const float cy2 = lerpf( y_out, center.y, wind_ctrl_k * ( 0.68f + h2 * 0.32f ) );

				const float x_src = -wind_x_pad * scale;
				const float x_dst = w + wind_x_pad * scale;

				const v2_t wp0 { x_src,     y_in  };
				const v2_t wp1 { w * 0.26f, cy1   };
				const v2_t wp2 { w * 0.74f, cy2   };
				const v2_t wp3 { x_dst,     y_out };

				const float wlen   = 0.09f + h3 * 0.17f;
				const float wspeed = 0.10f + h1 * 0.10f;
				const float period = 1.f + wlen;
				const float phase  = std::fmod( t * wspeed + h0 * period, period );

				const float whead = clamp01( phase );
				const float wtail = clamp01( phase - wlen );

				const float fi = clamp01( phase / 0.14f );
				const float fo = clamp01( ( period - phase ) / 0.14f );
				const float wa = ( std::min )( fi, fo );

				if ( wa <= 0.005f ) continue;

				draw_bezier_strand(
					wp0, wp1, wp2, wp3,
					wind_out, wind_in,
					alpha * wa * 0.82f,
					wtail, whead,
					1.7f, 0.5f,
					44, 0.48f );
			}
		}

		{
			const float p         = ease_out_cubic( phase01( t, 0.4f, 1.6f ) );
			const float logo_s    = lerpf( 0.88f, 1.f, p );
			const float pulse     = 0.9f + 0.1f * std::sin( t * 6.28318530718f / 3.f );
			const float la_scalar = alpha * p * pulse;
			const auto  la        = static_cast< std::uint8_t >( clamp01( la_scalar ) * 255.f );

			const float r_glow = 140.f * scale * logo_s;

			bypass::overlay::filled_circle(
				{ center.x, center.y }, r_glow,
				{ accent_violet.r, accent_violet.g, accent_violet.b,
				  static_cast< std::uint8_t >( la * 0.10f ) } );
			bypass::overlay::filled_circle(
				{ center.x, center.y }, r_glow * 0.72f,
				{ accent_blue.r, accent_blue.g, accent_blue.b,
				  static_cast< std::uint8_t >( la * 0.16f ) } );

			if ( logo_registered ) {
				const float logo_size = 220.f * scale * logo_s;
				bypass::overlay::image(
					logo_image_id,
					{ center.x - logo_size * 0.5f,
					  center.y - logo_size * 0.5f,
					  logo_size, logo_size },
					{ 255, 255, 255, la },
					clamp01( la_scalar ),
					0.f );
			}
			else {
				const float r_outer = 96.f * scale * logo_s;
				const float r_inner = 58.f * scale * logo_s;
				const float r_core  = 32.f * scale * logo_s;
				bypass::overlay::circle(
					{ center.x, center.y }, r_outer,
					{ accent_blue.r, accent_blue.g, accent_blue.b, la },
					1.5f );
				bypass::overlay::filled_circle(
					{ center.x, center.y }, r_inner,
					{ accent_soft.r, accent_soft.g, accent_soft.b,
					  static_cast< std::uint8_t >( la * 0.55f ) } );
				bypass::overlay::filled_circle(
					{ center.x, center.y }, r_core,
					{ text_light.r, text_light.g, text_light.b, la } );
			}
		}

		{
			const float p = ease_out_cubic( phase01( t, 2.2f, 3.2f ) );
			const auto  a = static_cast< std::uint8_t >( clamp01( alpha * p ) * 255.f );
			const float y = center.y + 220.f * scale;

			std::wstring welcome_line = L"Welcome back, ";
			welcome_line += menu::g_discord_name.empty( )
				? L"operator"
				: menu::g_discord_name;

			bypass::overlay::text_async(
				welcome_line.c_str( ),
				{ center.x - 400.f, y, 800.f, 40.f },
				{ text_light.r, text_light.g, text_light.b, a },
				26.f * scale,
				text_weight_t::medium,
				L"FiraCode-Medium",
				text_align_t::center,
				false );
		}

		{
			const float p = ease_out_cubic( phase01( t, 2.8f, 3.6f ) );
			const auto  a = static_cast< std::uint8_t >( clamp01( alpha * p * 0.85f ) * 255.f );
			const float y = center.y + 258.f * scale;
			bypass::overlay::text_async(
				L"leproxy  ·  aaddpp  ·  vmclear  ·  pointy  ·  x",
				{ center.x - 500.f, y, 1000.f, 30.f },
				{ text_muted.r, text_muted.g, text_muted.b, a },
				14.f * scale,
				text_weight_t::regular,
				L"FiraCode-Light",
				text_align_t::center,
				false );
		}

		{
			const float p_reveal = ease_out_cubic( phase01( t, 3.4f, 4.2f ) );
			const auto  wa       = static_cast< std::uint8_t >( clamp01( alpha * p_reveal ) * 255.f );

			const float bottom_gap    = 26.f * scale;
			const float underline_h   = 2.f;
			const float text_h        = 30.f * scale;
			const float underline_gap = 6.f  * scale;

			const float underline_y = h - bottom_gap - underline_h;
			const float wy          = underline_y - underline_gap - text_h;

			bypass::overlay::text_async(
				L"discord.gg/driverless",
				{ center.x - 400.f, wy, 800.f, text_h },
				{ accent_blue.r, accent_blue.g, accent_blue.b, wa },
				18.f * scale,
				text_weight_t::medium,
				L"FiraCode-Medium",
				text_align_t::center,
				false );

			const float underline_full = 280.f * scale;
			const float underline_w    = underline_full * p_reveal;
			const float underline_x    = center.x - underline_w * 0.5f;
			const auto  ua             = static_cast< std::uint8_t >( clamp01( alpha * p_reveal * 0.75f ) * 255.f );
			bypass::overlay::filled_rectangle(
				{ underline_x, underline_y, underline_w, underline_h },
				{ accent_blue.r, accent_blue.g, accent_blue.b, ua } );
		}

		if ( !g_wav_triggered && t >= k_wav_trigger_t ) {
			g_wav_triggered = true;
			if ( mciSendStringW(
					L"open \"C:\\crisis\\crisis.wav\" type waveaudio alias crisisbgm",
					nullptr, 0, nullptr ) == 0 ) {
				mciSendStringW( L"set crisisbgm volume 700", nullptr, 0, nullptr );
				wchar_t lenbuf[ 32 ] = { };
				mciSendStringW( L"status crisisbgm length", lenbuf, 32, nullptr );
				g_wav_len_ms = static_cast< int >( std::wcstol( lenbuf, nullptr, 10 ) );
				if ( mciSendStringW( L"play crisisbgm from 0", nullptr, 0, nullptr ) == 0 ) {
					g_wav_opened     = true;
					g_wav_play_start = std::chrono::steady_clock::now( );
					g_notif_done     = false;
					menu::notifications::push(
						L"Throw ya’ Dracos in the air,",
						L"This that Crisis Shit!",
						L"",
						bypass::overlay::color_t { 76, 139, 255, 255 },
						true, true );
				}
			}
		}

		if ( g_wav_opened && !g_notif_released ) {
			const auto elapsed_ms = static_cast< int >(
				std::chrono::duration_cast< std::chrono::milliseconds >(
					std::chrono::steady_clock::now( ) - g_wav_play_start ).count( ) );
			if ( g_wav_len_ms > 0 && elapsed_ms >= g_wav_len_ms ) {
				menu::notifications::finish_pinned( );
				g_notif_released   = true;
				g_notif_release_time = std::chrono::steady_clock::now( );
			}
		}

		if ( g_notif_released && !g_notif_done ) {
			const float ft = std::chrono::duration< float >(
				std::chrono::steady_clock::now( ) - g_notif_release_time ).count( );
			if ( ft >= 0.45f ) {
				g_notif_done = true;
			}
		}

		menu::notifications::render_pass( );
	}

	inline void draw_interactive( float ) {
		widgets::render( );
		menu::render( s_menu_enabled );
	}

	inline void init( ) {
		if ( inited ) return;
		inited = true;

		logo_registered = bypass::overlay::register_image(
			logo_image_id, L"C:\\crisis\\logo-only.png" );

		welcome_idx     = stages::register_stage(
			{ "welcome",     stages::stage_type_t::display,     true, draw_welcome     } );
		interactive_idx = stages::register_stage(
			{ "interactive", stages::stage_type_t::interactive, true, draw_interactive } );

		reset( );
	}
} // namespace gui::demo

namespace gui {
	void render( ) {
		using clock = std::chrono::steady_clock;
		cache::initialize_monitor_refresh_rate( );
		std::uint32_t cached_refresh_rate = cache::monitor_refresh_rate.load( std::memory_order_relaxed );
		clock::time_point last_refresh_check { };
		clock::time_point next_allowed_frame { };
		clock::time_point next_allowed_aimbot { };
		bool overlay_cleared_while_unfocused = false;

		timeBeginPeriod( 1 );

		const auto query_monitor_refresh_rate = [ ]( ) -> std::uint32_t {
			const HWND desktop = GetDesktopWindow( );
			const HMONITOR mon = MonitorFromWindow( desktop, MONITOR_DEFAULTTOPRIMARY );
			if ( !mon ) return 60;
			MONITORINFOEXW mi { };
			mi.cbSize = sizeof( mi );
			if ( !GetMonitorInfoW( mon, &mi ) ) return 60;
			DEVMODEW dm { };
			dm.dmSize = sizeof( dm );
			if ( !EnumDisplaySettingsW( mi.szDevice, ENUM_CURRENT_SETTINGS, &dm ) ) return 60;
			const auto hz = static_cast< std::uint32_t >( dm.dmDisplayFrequency );
			return ( hz > 1 && hz <= 1000 ) ? hz : 60;
			};

		const auto query_refresh_rate = [ &query_monitor_refresh_rate ]( const HWND hwnd ) -> std::uint32_t {
			if ( !hwnd || !IsWindow( hwnd ) )
				return query_monitor_refresh_rate( );
			const HMONITOR mon = MonitorFromWindow( hwnd, MONITOR_DEFAULTTONEAREST );
			if ( !mon )
				return query_monitor_refresh_rate( );
			MONITORINFOEXW mi { };
			mi.cbSize = sizeof( mi );
			if ( !GetMonitorInfoW( mon, &mi ) )
				return query_monitor_refresh_rate( );
			DEVMODEW dm { };
			dm.dmSize = sizeof( dm );
			if ( !EnumDisplaySettingsW( mi.szDevice, ENUM_CURRENT_SETTINGS, &dm ) )
				return query_monitor_refresh_rate( );
			const auto hz = static_cast< std::uint32_t >( dm.dmDisplayFrequency );
			return ( hz > 1 && hz <= 1000 ) ? hz : query_monitor_refresh_rate( );
			};

		priority::c_gate aimbot_gate;
		MSG message {};
		while ( true ) {
			//while ( PeekMessageW( &message, nullptr, 0, 0, PM_REMOVE ) ) {
			//	TranslateMessage( &message );
			//	DispatchMessageW( &message );
			//}

			const HWND target_window = bypass::target::m_window_handle;
			if ( !target_window || !IsWindow( target_window ) ) {
				bypass::overlay::clear( );
				bypass::overlay::present( );
				logging::print( obf( "fortnite stopped running.. press f7 when running." ) );
				while ( !( GetAsyncKeyState( VK_F7 ) & 1 ) )
					std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
				bypass::target::setup( );
				demo::reset( );
				continue;
			}

			const auto now = clock::now( );

			const auto cap_interval = ( config::settings::frame_limiter && config::settings::framerate_cap > 1.0f )
				? std::chrono::duration_cast< clock::duration >(
					std::chrono::duration< double >( 1.0 / static_cast< double >( config::settings::framerate_cap ) ) )
				: std::chrono::duration_cast< clock::duration >(
					std::chrono::duration< double >( 1.0 / static_cast< double >( cached_refresh_rate ) ) );

			if ( now < next_allowed_frame ) {
				const auto target = next_allowed_frame;
				while ( clock::now( ) < target )
					_mm_pause( );
			}
			next_allowed_frame = clock::now( ) + cap_interval;

			if ( now - last_refresh_check >= std::chrono::seconds( 1 ) ) {
				cached_refresh_rate = query_refresh_rate( target_window );
				last_refresh_check = now;
			}

			const HWND foreground = GetForegroundWindow( );
			const bool focused = foreground && target_window &&
				( foreground == target_window ||
					GetAncestor( foreground, GA_ROOT ) ==
					GetAncestor( target_window, GA_ROOT ) );

			if ( !focused ) {
				if ( !overlay_cleared_while_unfocused ) {
					bypass::overlay::clear( );
					bypass::overlay::present( );
					overlay_cleared_while_unfocused = true;
				}
				std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
				continue;
			}
			overlay_cleared_while_unfocused = false;

			bypass::overlay::clear( );
			demo::init( );

			if ( stages::g_current == demo::welcome_idx &&
			     stages::g_next == -1 ) {
				if ( demo::content_running ) {
					const float since_content =
						std::chrono::duration< float >(
							std::chrono::steady_clock::now( ) - demo::content_start ).count( );
					if ( since_content > demo::welcome_total_seconds && demo::g_notif_done ) {
						stages::switch_to( demo::interactive_idx, true );
						demo::apply_menu_bg_target( );
					}
				}
			}

			if ( GetAsyncKeyState( menu::menu_key( ) ) & 1 )
				demo::s_menu_enabled = !demo::s_menu_enabled;
			if ( demo::s_menu_enabled != demo::s_menu_prev ) {
				demo::apply_menu_bg_target( );
				demo::s_menu_prev = demo::s_menu_enabled;
			}

			stages::tick_and_draw_background( );

			cache::camera_snapshot_t frame_camera { };
			if ( !g_cache->sample_camera( frame_camera ) )
				frame_camera = cache::get_camera_snapshot( );
			else {
				std::unique_lock lock( cache::camera_snapshot_mutex );
				cache::camera_snapshot = frame_camera;
			}
			cache::apply_camera_snapshot( frame_camera );

			auto visual_snapshot = cache::get_visual_snapshot( );
			if ( visual_snapshot ) {
				const auto& players_copy = visual_snapshot->players;
				g_stats->update_player_list( players_copy );

				std::unordered_map <
					std::uint64_t,
					std::shared_ptr< player_stats::player_stats_cache_t >
				> stats_snapshot;

				if ( config::player::stats::kill_deaths || config::player::stats::battle_pass ) {
					std::lock_guard map_lock( player_stats::g_player_cache_map_mutex );
					stats_snapshot.reserve( player_stats::g_player_cache.size( ) );
					for ( auto& [key, value] : player_stats::g_player_cache ) {
						stats_snapshot.emplace(
							key,
							value.load( std::memory_order_acquire ) );
					}
				}

				g_aimbot->start( );
				for ( const auto& player : players_copy ) {
					auto render_player = player;
					const auto player_key =
						reinterpret_cast< std::uint64_t >( render_player.m_mesh );
					const auto stats_it = stats_snapshot.find( player_key );
					player::render(
						render_player,
						stats_it != stats_snapshot.end( )
						? stats_it->second
						: nullptr );
				}
			}

			auto items = g_cache->get_items( );
			if ( items ) {
				if ( config::loot::pickup && !items->pickups.empty( ) ) {
					static constexpr std::size_t max_rendered_pickups = 50;
					const auto& pickups = items->pickups;

					if ( pickups.size( ) <= max_rendered_pickups ) {
						for ( const auto& pawn : pickups )
							loot::render( pawn );
					} else {
						std::vector< std::size_t > indices( pickups.size( ) );
						for ( std::size_t j = 0; j < indices.size( ); ++j )
							indices[ j ] = j;
						std::partial_sort( indices.begin( ),
							indices.begin( ) + max_rendered_pickups,
							indices.end( ),
							[ & ]( std::size_t a, std::size_t b ) {
								return cache::camera_location.distance_to_sq(
									pickups[ a ].m_relative_location ) <
									cache::camera_location.distance_to_sq(
										pickups[ b ].m_relative_location );
							} );
						for ( std::size_t i = 0; i < max_rendered_pickups; ++i )
							loot::render( pickups[ indices[ i ] ] );
					}
				}

				if ( config::loot::chest && !items->chests.empty( ) ) {
					static constexpr std::size_t max_rendered_chests = 30;
					const auto& chests = items->chests;

					if ( chests.size( ) <= max_rendered_chests ) {
						for ( const auto& pawn : chests )
							chest::render( pawn );
					}
					else {
						std::vector< std::size_t > indices( chests.size( ) );
						for ( std::size_t j = 0; j < indices.size( ); ++j )
							indices[ j ] = j;
						std::partial_sort( indices.begin( ),
							indices.begin( ) + max_rendered_chests,
							indices.end( ),
							[ & ]( std::size_t a, std::size_t b ) {
								return cache::camera_location.distance_to_sq(
									chests[ a ].m_relative_location ) <
									cache::camera_location.distance_to_sq(
										chests[ b ].m_relative_location );
							} );
						for ( std::size_t i = 0; i < max_rendered_chests; ++i )
							chest::render( chests[ indices[ i ] ] );
					}
				}

				for ( const auto& pawn : items->weakspots )
					weakspot::render( pawn );
			}

			g_aimbot->render( );

			g_aimbot->target( );
			g_aimbot->trigger( );

			{
				auto tracers_copy = g_cache->get_bullet_tracers( );
				tracer::render( tracers_copy );
			}

			esp::tick_threat_arrow_anim( );
			esp::tracers::tick( );

			stages::draw_content( );
			bypass::overlay::present( );
		}
	}
}
