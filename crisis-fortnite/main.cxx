#include <deps\includes.h>

std::unique_ptr< cache::c_cache > g_cache =
	std::make_unique< cache::c_cache >( );
std::unique_ptr< stats::c_stats > g_api =
	std::make_unique< stats::c_stats >( );
std::unique_ptr< xp_system::c_system > g_system =
	std::make_unique< xp_system::c_system >( );
std::unique_ptr< aimbot::c_aimbot > g_aimbot =
	std::make_unique< aimbot::c_aimbot >( );
std::unique_ptr< gui::esp::health::c_health_thread > g_health_thread =
	std::make_unique< gui::esp::health::c_health_thread >( );
std::unique_ptr< player_stats::c_player_stats_thread > g_stats =
	std::make_unique< player_stats::c_player_stats_thread >( );

static std::wstring download_discord_avatar( const std::wstring& url ) {
	wchar_t temp_path[ MAX_PATH ] {};
	if ( !GetTempPathW( MAX_PATH, temp_path ) )
		return {};

	const std::wstring path = std::wstring( temp_path ) + L"crisis-menu-discord-avatar.png";
	return SUCCEEDED( URLDownloadToFileW( nullptr, url.c_str( ), path.c_str( ), 0, nullptr ) )
		? path : std::wstring {};
}

static void register_fonts( ) {
	static constexpr std::array<std::pair<const wchar_t*, const wchar_t*>, 19> fonts = { {
		{ L"Baloo2",                L"Baloo2.ttf" },
		{ L"FiraCode-Medium",       L"FiraCode-Medium.ttf" },
		{ L"FiraCode-Light",        L"FiraCode-Light.ttf" },
		{ L"FiraCode-SemiBold",     L"FiraCode-SemiBold.ttf" },
		{ L"FiraCode-Bold",         L"FiraCode-Bold.ttf" },
		{ L"SourceCodePro-Bold",    L"SourceCodePro-Bold.ttf" },
		{ L"TikTokSans-Bold",       L"TikTokSans-Bold.ttf" },
		{ L"TikTokSans-Regular",    L"TikTokSans-Regular.ttf" },
		{ L"SourceCodePro-Black",   L"SourceCodePro-Black.ttf" },
		{ L"Font Awesome",          L"Font Awesome 7 Free-Solid-900.otf" },
		{ L"Font Awesome Brands",   L"Font Awesome 7 Brands-Regular-400.otf" },
		{ L"Inter-Medium",          L"Inter_28pt-Medium.ttf" },
		{ L"Inter-Bold",            L"Inter_18pt-Bold.ttf" },
		{ L"Inter-Regular",         L"Inter_28pt-Regular.ttf" },
		{ L"Geist-Regular",         L"Geist-Regular.ttf" },
		{ L"Geist-Medium",          L"Geist-Medium.ttf" },
		{ L"Geist-Bold",            L"Geist-Bold.ttf" },
		{ L"SourceCodePro-Medium",  L"SourceCodePro-Medium.ttf" },
		{ L"SourceCodePro-Regular", L"SourceCodePro-Regular.ttf" }
	} };

	for ( const auto& [name, file] : fonts ) {
		std::wstring path = L"C:\\crisis\\";
		path += file;

		bypass::overlay::register_font( name, path.c_str( ) );
	}
}

static DWORD WINAPI fortnite_thread( LPVOID ) {
	SetThreadPriority( GetCurrentThread( ), THREAD_PRIORITY_ABOVE_NORMAL );

	using time_begin_period_t = MMRESULT( WINAPI* )( UINT );
	if ( const auto winmm = GetModuleHandleW( L"winmm.dll" ) ) {
		if ( const auto fn = reinterpret_cast< time_begin_period_t >(
				GetProcAddress( winmm, "timeBeginPeriod" ) ) )
			fn( 1 );
	}
	else if ( const auto winmm2 = LoadLibraryW( L"winmm.dll" ) ) {
		if ( const auto fn = reinterpret_cast< time_begin_period_t >(
				GetProcAddress( winmm2, "timeBeginPeriod" ) ) )
			fn( 1 );
	}

	CoInitializeEx( nullptr, COINIT_MULTITHREADED );
	crisis::casino::client.configure_from_environment( );
	crisis::profile::client.configure_from_environment( );
	crisis::profile::client.refresh( );
	SetProcessDpiAwarenessContext( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );
	 
	if ( !bypass::open_shared_data( ) ) {
		logging::print( obf ( "failed to open shared data" ) );
		return 1;
	}

	bypass::initialize(
		obf( L"FortniteClient-Win64-Shipping.exe" ),
		obf( L"" ) );

	if ( !bypass::create( ) ) {
		logging::print( obf( "failed to create bypass" ) );
		return 1;
	}

	logging::print( obf( "parsing weapons.." ) );
	logging::print( obf( "this may take a couple seconds." ) );

	if ( !g_api->load_weapons( ) ) {
		logging::print( obf( "downloading weapons" ) );
		g_api->download_weapons( );
	}

	logging::print( obf( "parsed weapons!" ) );

	if ( !bypass::target::setup( ) ) {
		logging::print( obf( "failed to setup target" ) );
		return 1;
	}

	g_cache->setup( );
	g_cache->initialize( );

	g_api->m_api_key = std::wstring( obf( L"0b9e68cf-8e7e-42a7-aaff-937226d4f111" ) );
	g_api->setup( );

	g_stats->start( );
	g_health_thread->start( );
	g_system->start( );

	const auto expires_unix = std::chrono::duration_cast< std::chrono::seconds >(
		std::chrono::system_clock::now( ).time_since_epoch( ) ).count( ) + 24 * 60 * 60;

	if ( const auto profile = discord::current_profile( ) ) {
		const std::wstring avatar_path = download_discord_avatar( profile->avatar_url );
		const bool image_registered = !avatar_path.empty( )
			&& bypass::overlay::register_image( menu::profile_image_id, avatar_path.c_str( ) );
		menu::set_profile( profile->display_name, profile->username, image_registered,
			profile->id, L"Developer", expires_unix );
	}
	else {
		menu::set_profile( L"User", L"user", false, L"", L"Developer", expires_unix );
	}

	register_fonts( );

	bypass::overlay::get_size( gui::screen_width, gui::screen_height );
	gui::screen_width_center  = gui::screen_width  / 2;
	gui::screen_height_center = gui::screen_height / 2;

	logging::print( obf( "external framework initialized" ) );
	gui::render( );
	return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule, DWORD ul_reason_for_call, LPVOID ) {
	if ( ul_reason_for_call == DLL_PROCESS_ATTACH ) {
		DisableThreadLibraryCalls( hModule );
		CreateThread( nullptr, 4 * 1024 * 1024, fortnite_thread, nullptr,
			STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr );
	}
	return TRUE;
}
